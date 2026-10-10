#include "record/Recorder.h"

#include "core/AppPaths.h"
#include "net/LinkConfig.h"
#include "proto/Dataframe.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <new>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

using namespace RecordFormat;

namespace {

constexpr int kTickMs = 250;
// Header ghi lại mỗi giây chứ không mỗi nhịp: ghi đè 256 byte đầu file là ghi
// lại cả một trang của SSD.
constexpr int kHeaderEveryTicks = 4;
constexpr int kCleanupEveryTicks = 60000 / kTickMs;
// Bộ đệm chờ ghi vượt ngưỡng này nghĩa là ổ đĩa không theo kịp (VIDEO_I đầy
// đủ chỉ ~0,25 MB/s): bỏ gói, dừng ghi lưu thay vì để bộ nhớ phình mãi.
constexpr int kMaxPendingBytes = 64 * 1024 * 1024;
constexpr double kGb = 1024.0 * 1024.0 * 1024.0;

const char *const kDeleteLog = "deletted_history.log"; // tên đúng như step-07

} // namespace

// --------------------------------------------------------------------- sink

RecordSink::RecordSink()
{
    m_clock.start();
}

void RecordSink::push(int stream, const char *data, int size)
{
    if (!isActive() || size <= 0 || size > kMaxPacketBytes)
        return;
    QMutexLocker lock(&m_mutex);
    if (!m_accepting || stream < 0 || stream >= m_streams.size())
        return;
    const Stream &s = m_streams.at(stream);
    if (s.policy == Skip)
        return;
    const int kind = kindForPacket(s.kind, data, size, s.bigEndian);
    quint8 flags = 0;
    if (s.policy == Short && size > kShortVideoBytes) {
        size = kShortVideoBytes;
        flags = PacketTruncated;
    }
    if (m_pending.size() + kPacketHeadBytes + size > kMaxPendingBytes) {
        m_counts.overflow = true;
        m_accepting = false;
        m_active.store(false);
        return;
    }
    const quint32 ms = quint32(qMax<qint64>(0, m_clock.elapsed() - m_fileStartMs));
    try {
        appendPacket(&m_pending, ms, quint8(stream), flags, data, size);
    } catch (const std::bad_alloc &) {
        // Hết bộ nhớ thì coi như tràn bộ đệm: luồng ghi báo lỗi và dừng ghi lưu.
        m_counts.overflow = true;
        m_accepting = false;
        m_active.store(false);
        return;
    }
    ++m_counts.count;
    m_counts.lastMs = ms;
    ++m_counts.kinds[size_t(kind)];
    ++m_filePackets;
    m_fileBytes += quint64(kPacketHeadBytes + size);
}

RecordStats RecordSink::stats() const
{
    QMutexLocker lock(&m_mutex);
    RecordStats st;
    st.active = m_accepting;
    st.fileName = m_fileName;
    st.elapsedMs = m_accepting ? m_clock.elapsed() - m_fileStartMs : 0;
    st.packets = m_filePackets;
    st.bytes = m_fileBytes;
    return st;
}

void RecordSink::setPolicies(const RecordSetup &setup)
{
    QMutexLocker lock(&m_mutex);
    for (Stream &s : m_streams) {
        if (!s.recv) {
            s.policy = Skip;
            continue;
        }
        switch (s.kind) {
        case KindVideoR: s.policy = setup.fullVideoR ? Full : Short; break;
        case KindVideoI: s.policy = setup.fullVideoI ? Full : Short; break;
        case KindRawIq:  s.policy = setup.rawIq ? Full : Skip; break;
        default:         s.policy = Full; break;
        }
    }
}

void RecordSink::open(qint64 startMs, const QString &fileName, quint64 fileBytes)
{
    QMutexLocker lock(&m_mutex);
    m_pending.clear();
    m_counts = Take();
    m_fileStartMs = startMs;
    m_fileName = fileName;
    m_filePackets = 0;
    m_fileBytes = fileBytes;
    m_accepting = true;
    m_active.store(true);
}

void RecordSink::setFileName(const QString &fileName, quint64 fileBytes)
{
    QMutexLocker lock(&m_mutex);
    m_fileName = fileName;
    m_fileBytes += fileBytes;
}

RecordSink::Take RecordSink::take(bool rotate)
{
    QByteArray fresh;
    // Cấp phát ngoài mutex, cỡ vừa một nhịp VIDEO_I đầy đủ.
    fresh.reserve(256 * 1024);
    QMutexLocker lock(&m_mutex);
    Take t = m_counts;
    t.packets.swap(m_pending);
    m_pending.swap(fresh);
    m_counts = Take();
    if (rotate) {
        t.newStartMs = m_clock.elapsed();
        t.newStartWall = QDateTime::currentDateTime();
        m_fileStartMs = t.newStartMs;
        m_filePackets = 0;
        m_fileBytes = 0;
    }
    return t;
}

RecordSink::Take RecordSink::close()
{
    QMutexLocker lock(&m_mutex);
    m_accepting = false;
    m_active.store(false);
    Take t = m_counts;
    t.packets.swap(m_pending);
    m_counts = Take();
    return t;
}

void RecordSink::addBytes(quint64 n)
{
    QMutexLocker lock(&m_mutex);
    m_fileBytes += n;
}

// ------------------------------------------------------------------- writer

RecordWriter::RecordWriter(std::shared_ptr<RecordSink> sink)
    : m_sink(std::move(sink))
{
}

RecordWriter::~RecordWriter()
{
    // Recorder::stop() đã đóng file; còn mở là đường thoát bất thường, vẫn cố
    // ghi header để file không mất số liệu.
    if (m_file)
        closeFile(true);
}

QString RecordWriter::beginSession(const RecordSetup &setup, const QJsonArray &streams)
{
    if (m_file)
        return QString();
    m_setup = setup;
    m_streams = streams;
    m_maxBytes = quint64(setup.maxSizeGb * kGb);
    m_maxMs = qint64(setup.maxTimeH * 3600.0 * 1000.0);
    m_capWarned = false;
    m_sink->setPolicies(setup);

    const qint64 start = m_sink->nowMs();
    QString error;
    if (!openFile(start, QDateTime::currentDateTime(), &error))
        return error;
    m_sink->open(start, m_relName, m_written);
    emit fileOpened(m_relName);

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setInterval(kTickMs);
        connect(m_timer, &QTimer::timeout, this, &RecordWriter::tick);
    }
    m_timer->start();
    // Xoá file cũ ngay từ đầu (total_cap có thể vừa bị hạ), nhưng sau khi trả
    // lời: giao diện đang chờ lời gọi này.
    QTimer::singleShot(0, this, &RecordWriter::cleanup);
    return QString();
}

QString RecordWriter::endSession()
{
    if (!m_file)
        return QString();
    if (m_timer)
        m_timer->stop();
    const RecordSink::Take t = m_sink->close();
    QString error;
    if (!writeTake(t, &error)) {
        closeFile(false);
        return error;
    }
    if (!writeHeader(true, &error)) {
        closeFile(false);
        return error;
    }
    closeFile(false);
    return QString();
}

void RecordWriter::tick()
{
    if (!m_file)
        return;
    const bool rotate = m_written >= m_maxBytes || m_sink->nowMs() - m_fileStartMs >= m_maxMs;
    // Đổi bộ đệm và (nếu ngắt file) đặt mốc file mới trong cùng một lần khoá:
    // gói nào cũng mang thời điểm tính từ đầu đúng file chứa nó.
    const RecordSink::Take t = m_sink->take(rotate);
    QString error;
    if (!writeTake(t, &error)) {
        fail(error);
        return;
    }
    if (t.overflow) {
        fail(QStringLiteral("Ghi lưu: ổ đĩa ghi không kịp (bộ đệm chờ vượt %1 MB), đã dừng ghi lưu ở file %2.")
                 .arg(kMaxPendingBytes / (1024 * 1024)).arg(m_relName));
        return;
    }
    if (rotate) {
        const QString oldName = m_relName;
        const QString why = (m_written >= m_maxBytes)
            ? QStringLiteral("đủ %1 MB").arg(double(m_written) / (1024.0 * 1024.0), 0, 'f', 1)
            : QStringLiteral("đủ %1 giờ").arg(m_setup.maxTimeH);
        if (!writeHeader(true, &error)) {
            fail(error);
            return;
        }
        closeFile(false);
        if (!openFile(t.newStartMs, t.newStartWall, &error)) {
            fail(error);
            return;
        }
        m_sink->setFileName(m_relName, m_written);
        emit message(QStringLiteral("Ghi lưu: file %1 %2, ghi tiếp sang file ./records/%3.")
                         .arg(oldName, why, m_relName),
                     false);
        cleanup();
        return;
    }
    ++m_ticks;
    if (m_ticks % kHeaderEveryTicks == 0 && !writeHeader(false, &error)) {
        fail(error);
        return;
    }
    if (m_ticks % kCleanupEveryTicks == 0)
        cleanup();
}

QString RecordWriter::writeError(const QString &what) const
{
    return QStringLiteral("Ghi lưu: lỗi %1 file ./records/%2 (%3), đã dừng ghi lưu.")
        .arg(what, m_relName, m_file ? m_file->errorString() : QString());
}

bool RecordWriter::openFile(qint64 startMs, const QDateTime &wall, QString *error)
{
    const QString root = AppPaths::recordsDir();
    const QString sub = wall.toString(QStringLiteral("yyyy/MM"));
    if (!QDir().mkpath(root + QLatin1Char('/') + sub)) {
        *error = QStringLiteral("Ghi lưu: không tạo được thư mục ./records/%1.").arg(sub);
        return false;
    }
    // Dừng rồi ghi lại ngay trong cùng một giây thì trùng tên: thêm hậu tố
    // chứ không bao giờ ghi đè file cũ.
    const QString base = sub + QLatin1Char('/') + wall.toString(QStringLiteral("yyyyMMdd_HHmmss"));
    QString rel = base + QStringLiteral(".rec");
    for (int n = 2; QFile::exists(root + QLatin1Char('/') + rel); ++n)
        rel = QStringLiteral("%1_%2.rec").arg(base).arg(n);

    auto *file = new QFile(root + QLatin1Char('/') + rel);
    if (!file->open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        *error = QStringLiteral("Ghi lưu: không tạo được file ./records/%1 (%2).").arg(rel, file->errorString());
        delete file;
        return false;
    }
    m_file = file;
    m_relName = rel;
    m_fileStartMs = startMs;
    m_ticks = 0;

    const qint64 wallMs = wall.toMSecsSinceEpoch();
    m_header = Header();
    m_header.f[FMagic] = kMagic;
    m_header.f[FStartSec] = quint32(wallMs / 1000);
    m_header.f[FStartMs] = quint32(wallMs % 1000);
    m_header.f[FEndSec] = m_header.f[FStartSec];
    m_header.f[FEndMs] = m_header.f[FStartMs];
    m_header.f[FVersion] = kVersion;
    m_header.f[FFlags] = (m_setup.fullVideoR ? FileFullVideoR : 0u) | (m_setup.fullVideoI ? FileFullVideoI : 0u)
                       | (m_setup.rawIq ? FileRawIq : 0u);
    m_header.f[FBlocks] = 1;

    // Khối mô tả: phát lại giải mã theo bảng dòng lúc ghi chứ không theo
    // connect.json của lúc phát (kỹ sư có thể đã đổi thứ tự byte, thêm dòng).
    QJsonObject meta;
    meta[QStringLiteral("format_version")] = int(kVersion);
    meta[QStringLiteral("mx18_version")] = QCoreApplication::applicationVersion();
    meta[QStringLiteral("start_local")] = wall.toString(Qt::ISODateWithMs);
    meta[QStringLiteral("utc_offset_s")] = wall.offsetFromUtc();
    meta[QStringLiteral("write_full_video_r")] = m_setup.fullVideoR;
    meta[QStringLiteral("write_full_video_i")] = m_setup.fullVideoI;
    meta[QStringLiteral("write_raw_iq")] = m_setup.rawIq;
    meta[QStringLiteral("streams")] = m_streams;
    const QByteArray json = QJsonDocument(meta).toJson(QJsonDocument::Compact);

    QByteArray block(kBlockHeadBytes, '\0');
    appendPacket(&block, 0, kMetaStream, 0, json.constData(), int(qMin<qsizetype>(json.size(), kMaxPacketBytes)));
    BlockHead head;
    head.bytes = quint32(block.size());
    head.packets = 1;
    writeBlockHead(block.data(), head);

    m_written = quint64(kHeaderBytes + block.size());
    m_header.f[FFileSize] = quint32(m_written);
    const QByteArray headerBytes = m_header.encode();
    if (m_file->write(headerBytes) != headerBytes.size() || m_file->write(block) != block.size()
        || !m_file->flush()) {
        *error = writeError(QStringLiteral("ghi"));
        closeFile(false);
        return false;
    }
    return true;
}

bool RecordWriter::writeTake(const RecordSink::Take &take, QString *error)
{
    if (take.count == 0 || take.packets.isEmpty())
        return true;
    BlockHead head;
    head.bytes = quint32(kBlockHeadBytes + take.packets.size());
    head.firstMs = Proto::readU32(take.packets.constData(), false);
    head.packets = take.count;
    char headBytes[kBlockHeadBytes];
    writeBlockHead(headBytes, head);
    if (m_file->write(headBytes, kBlockHeadBytes) != kBlockHeadBytes
        || m_file->write(take.packets) != take.packets.size() || !m_file->flush()) {
        *error = writeError(QStringLiteral("ghi"));
        return false;
    }
    m_written += head.bytes;
    m_sink->addBytes(kBlockHeadBytes);
    m_header.f[FTotal] += take.count;
    for (int k = 0; k < KindCount; ++k)
        m_header.f[size_t(FCountBase + k)] += take.kinds[size_t(k)];
    ++m_header.f[FBlocks];
    m_header.f[FDurationMs] = take.lastMs;
    m_header.f[FFileSize] = quint32(qMin<quint64>(m_written, 0xffffffffu));
    return true;
}

bool RecordWriter::writeHeader(bool closing, QString *error)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_header.f[FEndSec] = quint32(nowMs / 1000);
    m_header.f[FEndMs] = quint32(nowMs % 1000);
    if (closing)
        m_header.f[FClosed] = kClosedMark;
    const QByteArray bytes = m_header.encode();
    const qint64 end = m_file->pos();
    if (!m_file->seek(0) || m_file->write(bytes) != bytes.size() || !m_file->seek(end) || !m_file->flush()) {
        *error = writeError(QStringLiteral("ghi header"));
        return false;
    }
    return true;
}

void RecordWriter::closeFile(bool closing)
{
    if (!m_file)
        return;
    if (closing) {
        QString ignored;
        writeHeader(true, &ignored);
    }
    m_file->close();
    delete m_file;
    m_file = nullptr;
}

void RecordWriter::fail(const QString &text)
{
    if (m_timer)
        m_timer->stop();
    m_sink->close();
    if (m_file) {
        // Không đánh dấu "đóng đàng hoàng": khối cuối có thể ghi dở, danh sách
        // phát lại sẽ đi dọc các khối để lấy số liệu thật. Cập nhật giờ kết thúc
        // được thì tốt, lỗi tiếp cũng bỏ qua.
        QString ignored;
        writeHeader(false, &ignored);
        closeFile(false);
    }
    emit failed(text);
}

void RecordWriter::cleanup()
{
    const QString root = AppPaths::recordsDir();
    const QDir rootDir(root);
    struct Item {
        QString rel;
        qint64 size;
    };
    QVector<Item> items;
    qint64 total = 0;
    QDirIterator it(root, {QStringLiteral("*.rec")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        items.append({rootDir.relativeFilePath(fi.filePath()), fi.size()});
        total += fi.size();
    }
    const qint64 cap = qint64(m_setup.totalCapGb * kGb);
    if (total < cap)
        return;

    // Tên file là thời điểm bắt đầu ghi (yyyy/MM/yyyyMMdd_HHmmss) nên thứ tự
    // tên cũng là thứ tự thời gian, không phụ thuộc giờ sửa file.
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.rel < b.rel; });

    QFile log(root + QLatin1Char('/') + QLatin1String(kDeleteLog));
    const bool logOpen = log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    QTextStream out(&log);
    int deleted = 0;
    qint64 freed = 0;
    for (const Item &item : std::as_const(items)) {
        if (total < cap)
            break;
        if (item.rel == m_relName)
            continue;
        if (!QFile::remove(root + QLatin1Char('/') + item.rel)) {
            emit message(QStringLiteral("Ghi lưu: không xoá được file cũ ./records/%1.").arg(item.rel), true);
            continue;
        }
        total -= item.size;
        freed += item.size;
        ++deleted;
        if (logOpen) {
            out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << '\t'
                << item.rel << '\t' << item.size << '\n';
        }
        // Thư mục tháng / năm rỗng thì dọn luôn; rmdir không xoá thư mục còn file.
        const QString dir = QFileInfo(item.rel).path();
        rootDir.rmdir(dir);
        rootDir.rmdir(QFileInfo(dir).path());
    }
    if (logOpen) {
        out.flush();
        log.close();
    } else if (deleted > 0) {
        emit message(QStringLiteral("Ghi lưu: không ghi được ./records/%1 (%2).")
                         .arg(QLatin1String(kDeleteLog), log.errorString()), true);
    }
    if (deleted > 0) {
        emit message(QStringLiteral("Ghi lưu: đã xoá %1 file cũ nhất (%2 MB) để tổng dung lượng ./records "
                                    "dưới %3 GB.")
                         .arg(deleted)
                         .arg(double(freed) / (1024.0 * 1024.0), 0, 'f', 1)
                         .arg(m_setup.totalCapGb),
                     false);
    }
    if (total >= cap && !m_capWarned) {
        m_capWarned = true;
        emit message(QStringLiteral("Ghi lưu: ./records vẫn vượt total_cap = %1 GB dù đã xoá hết file cũ — "
                                    "total_cap nhỏ hơn một file ghi lưu?")
                         .arg(m_setup.totalCapGb),
                     true);
    }
}

// ----------------------------------------------------------------- facade

Recorder::Recorder(QObject *parent)
    : QObject(parent)
    , m_sink(std::make_shared<RecordSink>())
{
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("record-writer"));
    m_writer = new RecordWriter(m_sink);
    m_writer->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_writer, &QObject::deleteLater);
    connect(m_writer, &RecordWriter::message, this, &Recorder::message);
    connect(m_writer, &RecordWriter::fileOpened, this, &Recorder::fileOpened);
    connect(m_writer, &RecordWriter::failed, this, [this](const QString &text) {
        m_recording = false;
        emit failed(text);
    });
    m_thread->start();
}

Recorder::~Recorder()
{
    stop();
    m_thread->quit();
    m_thread->wait();
}

void Recorder::setStreams(const LinkConfig &config)
{
    QVector<RecordSink::Stream> streams;
    QJsonArray table;
    // Số dòng là một byte trong bản ghi, 0xff dành cho khối mô tả.
    const int n = qMin(int(config.entries.size()), int(kMetaStream));
    for (int i = 0; i < n; ++i) {
        const LinkEntry &e = config.entries.at(i);
        RecordSink::Stream s;
        s.kind = kindForStream(e.category, e.format);
        s.bigEndian = e.bigEndian;
        s.recv = (e.direction != LinkEntry::Send);
        streams.append(s);
        if (!s.recv)
            continue;
        QJsonObject o;
        o[QStringLiteral("index")] = i;
        o[QStringLiteral("category")] = e.category;
        o[QStringLiteral("format")] = LinkEntry::formatName(e.format);
        o[QStringLiteral("big_endian")] = e.bigEndian;
        o[QStringLiteral("protocol")] = (e.protocol == LinkEntry::Tcp) ? QStringLiteral("tcp")
                                                                        : QStringLiteral("udp");
        o[QStringLiteral("kind")] = kindName(s.kind);
        table.append(o);
    }
    QMutexLocker lock(&m_sink->m_mutex);
    m_sink->m_streams = streams;
    m_streams = table;
}

bool Recorder::start(const RecordSetup &setup, QString *error)
{
    if (m_recording)
        return true;
    QString e;
    QMetaObject::invokeMethod(
        m_writer, [&] { e = m_writer->beginSession(setup, m_streams); }, Qt::BlockingQueuedConnection);
    if (!e.isEmpty()) {
        if (error)
            *error = e;
        return false;
    }
    m_recording = true;
    return true;
}

QString Recorder::stop()
{
    if (!m_recording)
        return QString();
    QString e;
    QMetaObject::invokeMethod(m_writer, [&] { e = m_writer->endSession(); }, Qt::BlockingQueuedConnection);
    m_recording = false;
    return e;
}
