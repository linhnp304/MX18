#include "record/Replayer.h"

#include "core/DataClock.h"
#include "net/DataLink.h"
#include "net/LinkConfig.h"
#include "proto/Dataframe.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

using namespace RecordFormat;

namespace {

// Nhịp phát: video 400 gói/giây nên mỗi nhịp 1x có chừng chục gói mỗi dòng,
// đủ mịn so với nhịp vẽ 25 hình/giây của panel 1.
constexpr int kTickMs = 20;
// Giao diện chưa xử lý xong quá ngần này nhịp (100 ms dữ liệu ở 1x) thì luồng
// phát lại đứng chờ: chạy 8x trên máy yếu, hàng đợi tín hiệu không phình mãi.
constexpr quint64 kMaxLagTicks = 5;
// Luồng phát lại bị hệ điều hành cho nghỉ lâu thì không bù cả quãng một lúc.
constexpr qint64 kMaxStepMs = 200;

} // namespace

// ------------------------------------------------------------------ engine

ReplayEngine::ReplayEngine(std::shared_ptr<ReplayShared> shared, std::shared_ptr<RawSink> rawSink)
    : m_shared(std::move(shared))
    , m_rawSink(std::move(rawSink))
    , m_streamKind(256, KindOther)
    , m_streamBe(256, false)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &ReplayEngine::step);
}

ReplayEngine::~ReplayEngine()
{
    close();
}

QString ReplayEngine::open(const QString &path)
{
    close();
    auto *file = new QFile(path);
    // Không đệm: tua đọc thẳng từng khối, đi dọc khối chỉ đọc 16 byte mỗi khối.
    if (!file->open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        const QString error = file->errorString();
        delete file;
        return QStringLiteral("không mở được file (%1)").arg(error);
    }
    m_file = file;

    const QByteArray headBytes = m_file->read(kHeaderBytes);
    Header header;
    if (!header.decode(headBytes.constData(), int(headBytes.size()))) {
        close();
        return QStringLiteral("không phải file ghi lưu MX18");
    }
    QString error;
    const QJsonObject meta = QJsonDocument::fromJson(readMeta(m_file, &error)).object();
    const QJsonArray streams = meta.value(QStringLiteral("streams")).toArray();
    if (streams.isEmpty()) {
        close();
        return error.isEmpty() ? QStringLiteral("khối mô tả không có bảng dòng") : error;
    }
    if (!walkBlocks(m_file, &m_blocks, nullptr, nullptr, &error) || m_blocks.size() <= 1) {
        close();
        return error.isEmpty() ? QStringLiteral("file chưa có gói tin nào") : error;
    }
    m_durationMs = lastPacketMs(m_file, m_blocks.constLast());
    m_startWallMs = header.startTime().toMSecsSinceEpoch();

    // Mỗi dòng đã ghi một LinkWorker không mở socket: gói phát lại đi đúng
    // đường cắt / giải mã như lúc nhận thật, theo định dạng và thứ tự byte của
    // lúc ghi.
    for (const QJsonValue &v : streams) {
        const QJsonObject s = v.toObject();
        const int idx = s.value(QStringLiteral("index")).toInt(-1);
        const int format = LinkEntry::formatFromName(s.value(QStringLiteral("format")).toString());
        if (idx < 0 || idx >= int(kMetaStream) || format < 0)
            continue;
        LinkEntry e;
        e.category = s.value(QStringLiteral("category")).toString();
        e.direction = LinkEntry::Recv;
        e.protocol = (s.value(QStringLiteral("protocol")).toString() == QLatin1String("tcp")) ? LinkEntry::Tcp
                                                                                              : LinkEntry::Udp;
        e.format = format;
        e.bigEndian = s.value(QStringLiteral("big_endian")).toBool();
        m_streamKind[idx] = kindForStream(e.category, format);
        m_streamBe[idx] = e.bigEndian;

        auto *w = new LinkWorker(e, m_rawSink);
        connect(w, &LinkWorker::frameReceived, this, &ReplayEngine::frameReceived);
        connect(w, &LinkWorker::scnCfReceived, this, &ReplayEngine::scnCfReceived);
        connect(w, &LinkWorker::asterixReceived, this, &ReplayEngine::asterixReceived);
        connect(w, &LinkWorker::message, this, [this](const QString &text, bool isError) {
            emit message(QStringLiteral("Phát lại – %1").arg(text), isError);
        });
        m_workers.insert(idx, w);
    }

    m_counts = ReplayStats();
    m_counts.durationMs = m_durationMs;
    m_blockIdx = -1;
    m_havePeek = false;
    m_posMs = 0.0;
    m_paused = false;
    m_atEnd = false;
    m_seq = 0;
    m_real.start();
    m_timer->start();
    publish();
    return QString();
}

void ReplayEngine::close()
{
    m_timer->stop();
    qDeleteAll(m_workers);
    m_workers.clear();
    delete m_file;
    m_file = nullptr;
    m_blocks.clear();
    m_block.clear();
    m_blockIdx = -1;
    m_havePeek = false;
}

void ReplayEngine::setPaused(bool paused)
{
    if (!m_file)
        return;
    // "Tiếp tục" khi đã hết file thì phát lại từ đầu.
    if (!paused && m_atEnd)
        seek(0);
    m_paused = paused;
    m_real.restart();
    publish();
}

void ReplayEngine::setRate(double rate)
{
    m_rate = qBound(0.1, rate, 16.0);
}

void ReplayEngine::seek(qint64 ms)
{
    if (!m_file)
        return;
    const qint64 target = qBound<qint64>(0, ms, m_durationMs);
    // Khối cuối cùng có gói đầu không muộn hơn đích; khối 0 là khối mô tả.
    const auto it = std::upper_bound(m_blocks.cbegin() + 1, m_blocks.cend(), target,
                                     [](qint64 t, const BlockRef &b) { return t < qint64(b.firstMs); });
    const int idx = qMax(1, int(it - m_blocks.cbegin()) - 1);
    m_havePeek = false;
    m_blockIdx = -1;
    if (loadBlock(idx)) {
        while (peek() && qint64(m_peek.ms) < target)
            m_havePeek = false;
    }
    m_posMs = double(target);
    m_atEnd = false;
    if (!peek())
        markEnd();
    m_real.restart();

    emit seeked();
    publish();
    emit tick(++m_seq, qint64(m_posMs), m_startWallMs + qint64(m_posMs));
    if (m_atEnd)
        emit reachedEnd();
}

void ReplayEngine::markEnd()
{
    // Hết file thì đứng hẳn ở trạng thái tạm dừng (nút hiện "Tiếp tục"): tua lùi
    // lại vẫn đứng yên cho tới khi bấm "Tiếp tục", khớp với chữ trên nút.
    m_atEnd = true;
    m_paused = true;
    m_posMs = double(m_durationMs);
}

void ReplayEngine::step()
{
    const qint64 dt = qMin(m_real.restart(), kMaxStepMs);
    if (!m_file || m_paused || m_atEnd)
        return;
    if (m_seq > m_shared->acked.load(std::memory_order_relaxed) + kMaxLagTicks)
        return;

    m_posMs += double(dt) * m_rate;
    while (peek() && double(m_peek.ms) <= m_posMs) {
        deliver(m_peek);
        m_havePeek = false;
    }
    if (!peek())
        markEnd();
    publish();
    emit tick(++m_seq, qint64(m_posMs), m_startWallMs + qint64(m_posMs));
    if (m_atEnd)
        emit reachedEnd();
}

bool ReplayEngine::peek()
{
    if (m_havePeek)
        return true;
    for (;;) {
        if (m_blockIdx >= 0 && nextPacket(m_block, &m_offset, &m_peek)) {
            m_havePeek = true;
            return true;
        }
        if (!loadBlock(m_blockIdx + 1))
            return false;
    }
}

bool ReplayEngine::loadBlock(int index)
{
    if (index < 0 || index >= m_blocks.size())
        return false;
    const BlockRef &b = m_blocks.at(index);
    if (!m_file->seek(b.offset + kBlockHeadBytes))
        return false;
    m_block = m_file->read(b.bytes - kBlockHeadBytes);
    if (m_block.size() != int(b.bytes - kBlockHeadBytes))
        return false;
    m_blockIdx = index;
    m_offset = 0;
    m_havePeek = false;
    return true;
}

void ReplayEngine::deliver(const Packet &pk)
{
    if (pk.stream == kMetaStream)
        return;
    const bool be = m_streamBe.at(pk.stream);
    const int kind = kindForPacket(m_streamKind.at(pk.stream), pk.data, pk.size, be);
    ++m_counts.total;
    switch (kind) {
    case KindVideoR:
        ++m_counts.videoR;
        break;
    case KindVideoI:
        ++m_counts.videoI;
        break;
    case KindPlot:
        ++m_counts.plot;
        break;
    case KindVq:
        ++m_counts.track;
        break;
    case KindStatusOther:
        if (pk.size >= 8 && Proto::readU32(pk.data + 4, be) == Proto::CatTrack)
            ++m_counts.track;
        break;
    default:
        break;
    }
    // Chép ra QByteArray riêng: tín hiệu qua hàng đợi còn giữ dữ liệu sau khi
    // khối này đã bị thay bằng khối sau.
    if (LinkWorker *w = m_workers.value(pk.stream))
        w->inject(QByteArray(pk.data, pk.size));
}

void ReplayEngine::publish()
{
    m_counts.posMs = qint64(m_posMs);
    m_counts.wallMs = m_startWallMs + m_counts.posMs;
    m_counts.paused = m_paused;
    m_counts.atEnd = m_atEnd;
    QMutexLocker lock(&m_shared->mutex);
    m_shared->stats = m_counts;
}

// ----------------------------------------------------------------- mặt tiền

Replayer::Replayer(std::shared_ptr<RawSink> rawSink, QObject *parent)
    : QObject(parent)
    , m_rawSink(std::move(rawSink))
{
}

Replayer::~Replayer()
{
    stop();
}

bool Replayer::start(const QString &path, QString *error)
{
    stop();
    m_shared = std::make_shared<ReplayShared>();
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("replay"));
    m_engine = new ReplayEngine(m_shared, m_rawSink);
    m_engine->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_engine, &QObject::deleteLater);

    const quint64 gen = m_generation;
    connect(m_engine, &ReplayEngine::frameReceived, this,
            [this, gen](quint32 category, quint32 serial, const QByteArray &data, bool be) {
                if (gen == m_generation)
                    emit frameReceived(category, serial, data, be);
            });
    connect(m_engine, &ReplayEngine::scnCfReceived, this, [this, gen](const ScnCf::Message &m) {
        if (gen == m_generation)
            emit scnCfReceived(m);
    });
    connect(m_engine, &ReplayEngine::asterixReceived, this, [this, gen](const Asterix::Batch &b) {
        if (gen == m_generation)
            emit asterixReceived(b);
    });
    connect(m_engine, &ReplayEngine::message, this, [this, gen](const QString &text, bool isError) {
        if (gen == m_generation)
            emit message(text, isError);
    });
    connect(m_engine, &ReplayEngine::tick, this, [this, gen](quint64 seq, qint64 fileMs, qint64 wallMs) {
        if (gen != m_generation)
            return;
        // Mốc thời gian đến sau các gói của cùng nhịp (cùng hàng đợi, đúng thứ
        // tự gửi), nên đồng hồ dữ liệu không bao giờ chạy trước gói đã vẽ.
        m_shared->acked.store(seq, std::memory_order_relaxed);
        DataClock::setReplayTime(fileMs, wallMs);
    });
    connect(m_engine, &ReplayEngine::seeked, this, [this, gen] {
        if (gen != m_generation)
            return;
        DataClock::jump();
        emit seeked();
    });
    connect(m_engine, &ReplayEngine::reachedEnd, this, [this, gen] {
        if (gen == m_generation)
            emit reachedEnd();
    });

    m_thread->start();
    DataClock::beginReplay();
    QString e;
    QMetaObject::invokeMethod(
        m_engine,
        [&] {
            m_engine->setRate(m_rate);
            e = m_engine->open(path);
        },
        Qt::BlockingQueuedConnection);
    if (!e.isEmpty()) {
        stop();
        *error = e;
        return false;
    }
    return true;
}

void Replayer::stop()
{
    if (!m_thread)
        return;
    ++m_generation;
    ReplayEngine *engine = m_engine;
    QMetaObject::invokeMethod(engine, [engine] { engine->close(); }, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_engine = nullptr;
    DataClock::endReplay();
}

void Replayer::setPaused(bool paused)
{
    if (ReplayEngine *engine = m_engine)
        QMetaObject::invokeMethod(engine, [engine, paused] { engine->setPaused(paused); }, Qt::QueuedConnection);
}

void Replayer::seek(qint64 ms)
{
    if (ReplayEngine *engine = m_engine)
        QMetaObject::invokeMethod(engine, [engine, ms] { engine->seek(ms); }, Qt::QueuedConnection);
}

void Replayer::setRate(double rate)
{
    m_rate = rate;
    if (ReplayEngine *engine = m_engine)
        QMetaObject::invokeMethod(engine, [engine, rate] { engine->setRate(rate); }, Qt::QueuedConnection);
}

ReplayStats Replayer::stats() const
{
    if (!m_shared)
        return ReplayStats();
    QMutexLocker lock(&m_shared->mutex);
    return m_shared->stats;
}
