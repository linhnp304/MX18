#include "record/RecordFormat.h"

#include "net/LinkConfig.h"
#include "proto/Dataframe.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace RecordFormat {

namespace {

quint32 rd32(const char *p) { return Proto::readU32(p, false); }
void wr32(char *p, quint32 v) { Proto::writeU32(p, v, false); }

quint16 rd16(const char *p)
{
    const auto *u = reinterpret_cast<const uchar *>(p);
    return quint16(u[0] | (u[1] << 8));
}

void wr16(char *p, quint16 v)
{
    p[0] = char(v & 0xff);
    p[1] = char(v >> 8);
}

struct KindName {
    const char *stream;        // tên dòng trong connect.json
    const char *key;           // tên trong khối mô tả
};

// Cùng thứ tự với enum Kind.
const KindName kKinds[KindCount] = {
    {"Video-R",       "video_r"},
    {"Video-I",       "video_i"},
    {nullptr,         "plot"},
    {"Data-Status",   "status"},
    {"Params-Status", "params"},
    {"Data-RAW",      "raw_iq"},
    {"X18-SCN",       "scn"},
    {"X18-SCN-R",     "scn_r"},
    {"X18-VQ",        "vq"},
    {nullptr,         "other"},
};

int kindFromKey(const QString &key)
{
    for (int k = 0; k < KindCount; ++k) {
        if (key == QLatin1String(kKinds[k].key))
            return k;
    }
    return KindOther;
}

QDateTime fromParts(quint32 sec, quint32 ms)
{
    return QDateTime::fromMSecsSinceEpoch(qint64(sec) * 1000 + qMin<quint32>(ms, 999));
}

} // namespace

QByteArray Header::encode() const
{
    QByteArray out(kHeaderBytes, '\0');
    for (int i = 0; i < kHeaderFields; ++i)
        wr32(out.data() + i * 4, f[size_t(i)]);
    return out;
}

bool Header::decode(const char *data, int size)
{
    if (size < kHeaderBytes || rd32(data) != kMagic)
        return false;
    for (int i = 0; i < kHeaderFields; ++i)
        f[size_t(i)] = rd32(data + i * 4);
    return true;
}

QDateTime Header::startTime() const { return fromParts(f[FStartSec], f[FStartMs]); }
QDateTime Header::endTime() const { return fromParts(f[FEndSec], f[FEndMs]); }

qint64 Header::recordedMs() const
{
    return qMax<qint64>(0, startTime().msecsTo(endTime()));
}

int kindForStream(const QString &category, int format)
{
    // Dòng raw_iq nào cũng là RAW_IQ dù kỹ sư đặt tên gì.
    if (format == LinkEntry::RawIq)
        return KindRawIq;
    for (int k = 0; k < KindCount; ++k) {
        if (kKinds[k].stream && category.compare(QLatin1String(kKinds[k].stream), Qt::CaseInsensitive) == 0)
            return k;
    }
    return KindOther;
}

int kindForPacket(int streamKind, const char *data, int size, bool bigEndian)
{
    if (streamKind == KindStatusOther && size >= 8
        && Proto::readU32(data + 4, bigEndian) == Proto::CatPlot) {
        return KindPlot;
    }
    return streamKind;
}

QString kindName(int kind)
{
    if (kind < 0 || kind >= KindCount)
        return QString();
    return QString::fromLatin1(kKinds[kind].key);
}

void writeBlockHead(char *out, const BlockHead &head)
{
    wr32(out, head.sync);
    wr32(out + 4, head.bytes);
    wr32(out + 8, head.firstMs);
    wr32(out + 12, head.packets);
}

bool readBlockHead(const char *in, BlockHead *head)
{
    head->sync = rd32(in);
    head->bytes = rd32(in + 4);
    head->firstMs = rd32(in + 8);
    head->packets = rd32(in + 12);
    // Khối nhỏ nhất là một bản ghi rỗng; khối không có gói thì không bao giờ
    // được ghi ra.
    return head->sync == kBlockSync && head->bytes >= quint32(kBlockHeadBytes + kPacketHeadBytes)
        && head->packets > 0;
}

void appendPacket(QByteArray *out, quint32 ms, quint8 stream, quint8 flags,
                  const char *data, int size)
{
    char head[kPacketHeadBytes];
    wr32(head, ms);
    wr16(head + 4, quint16(size));
    head[6] = char(stream);
    head[7] = char(flags);
    out->append(head, kPacketHeadBytes);
    out->append(data, size);
}

bool nextPacket(const QByteArray &block, int *offset, Packet *out)
{
    const int at = *offset;
    if (at + kPacketHeadBytes > block.size())
        return false;
    const char *p = block.constData() + at;
    const int size = rd16(p + 4);
    if (at + kPacketHeadBytes + size > block.size())
        return false;
    out->ms = rd32(p);
    out->stream = quint8(p[6]);
    out->flags = quint8(p[7]);
    out->data = p + kPacketHeadBytes;
    out->size = size;
    *offset = at + kPacketHeadBytes + size;
    return true;
}

QByteArray readMeta(QIODevice *file, QString *error)
{
    if (!file->seek(kHeaderBytes)) {
        if (error)
            *error = QStringLiteral("không đọc được khối mô tả");
        return QByteArray();
    }
    char headBytes[kBlockHeadBytes];
    BlockHead head;
    if (file->read(headBytes, kBlockHeadBytes) != kBlockHeadBytes || !readBlockHead(headBytes, &head)) {
        if (error)
            *error = QStringLiteral("thiếu khối mô tả");
        return QByteArray();
    }
    const QByteArray block = file->read(head.bytes - kBlockHeadBytes);
    int offset = 0;
    Packet pk;
    if (!nextPacket(block, &offset, &pk) || pk.stream != kMetaStream) {
        if (error)
            *error = QStringLiteral("khối đầu không phải khối mô tả");
        return QByteArray();
    }
    return QByteArray(pk.data, pk.size);
}

bool walkBlocks(QIODevice *file, QVector<BlockRef> *blocks, qint64 *validBytes,
                Header *recount, QString *error)
{
    // Bảng dòng → loại và thứ tự byte, chỉ cần khi đếm lại.
    QVector<int> streamKind(256, KindOther);
    QVector<bool> streamBe(256, false);
    if (recount) {
        const QJsonObject meta = QJsonDocument::fromJson(readMeta(file, error)).object();
        const QJsonArray streams = meta.value(QStringLiteral("streams")).toArray();
        for (const QJsonValue &v : streams) {
            const QJsonObject s = v.toObject();
            const int idx = s.value(QStringLiteral("index")).toInt(-1);
            if (idx < 0 || idx >= 255)
                continue;
            streamKind[idx] = kindFromKey(s.value(QStringLiteral("kind")).toString());
            streamBe[idx] = s.value(QStringLiteral("big_endian")).toBool();
        }
        for (int k = 0; k < KindCount; ++k)
            recount->f[size_t(FCountBase + k)] = 0;
        recount->f[FTotal] = 0;
        recount->f[FDurationMs] = 0;
    }

    qint64 pos = kHeaderBytes;
    const qint64 fileSize = file->size();
    if (validBytes)
        *validBytes = pos;
    if (!file->seek(pos)) {
        if (error)
            *error = QStringLiteral("không đọc được file");
        return false;
    }
    char headBytes[kBlockHeadBytes];
    quint32 blockCount = 0;
    while (pos + kBlockHeadBytes <= fileSize) {
        BlockHead head;
        if (file->read(headBytes, kBlockHeadBytes) != kBlockHeadBytes || !readBlockHead(headBytes, &head)
            || pos + head.bytes > fileSize) {
            break; // khối cuối ghi dở
        }
        if (recount) {
            const QByteArray body = file->read(head.bytes - kBlockHeadBytes);
            if (body.size() != int(head.bytes - kBlockHeadBytes))
                break;
            int offset = 0;
            Packet pk;
            while (nextPacket(body, &offset, &pk)) {
                if (pk.stream == kMetaStream)
                    continue;
                const int kind = kindForPacket(streamKind[pk.stream], pk.data, pk.size, streamBe[pk.stream]);
                ++recount->f[size_t(FCountBase + kind)];
                ++recount->f[FTotal];
                recount->f[FDurationMs] = pk.ms;
            }
        } else if (!file->seek(pos + head.bytes)) {
            break;
        }
        if (blocks) {
            BlockRef ref;
            ref.offset = pos;
            ref.bytes = head.bytes;
            ref.firstMs = head.firstMs;
            ref.packets = head.packets;
            blocks->append(ref);
        }
        ++blockCount;
        pos += head.bytes;
        if (validBytes)
            *validBytes = pos;
    }
    if (recount)
        recount->f[FBlocks] = blockCount;
    return true;
}

quint32 lastPacketMs(QIODevice *file, const BlockRef &block)
{
    if (!file->seek(block.offset + kBlockHeadBytes))
        return 0;
    const QByteArray body = file->read(block.bytes - kBlockHeadBytes);
    int offset = 0;
    Packet pk;
    quint32 last = block.firstMs;
    while (nextPacket(body, &offset, &pk))
        last = pk.ms;
    return last;
}

bool summarize(const QString &path, Summary *out, QString *error)
{
    // Không đệm: đi dọc khối chỉ đọc 16 byte mỗi khối, đọc có đệm thì mỗi lần
    // nhảy kéo theo cả 16 KB.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray headBytes = file.read(kHeaderBytes);
    Header header;
    if (!header.decode(headBytes.constData(), int(headBytes.size()))) {
        if (error)
            *error = QStringLiteral("không phải file ghi lưu MX18");
        return false;
    }

    out->path = path;
    out->bytes = quint64(file.size());
    out->start = header.startTime();
    out->closed = header.closed();
    if (out->closed) {
        out->end = header.endTime();
        out->durationMs = header.recordedMs();
        out->packets = header.f[FTotal];
    } else {
        QVector<BlockRef> blocks;
        walkBlocks(&file, &blocks, nullptr, nullptr, error);
        quint64 packets = 0;
        for (const BlockRef &b : std::as_const(blocks))
            packets += b.packets;
        // Khối đầu chỉ có khối mô tả.
        out->packets = blocks.isEmpty() ? 0 : packets - 1;
        out->durationMs = blocks.size() > 1 ? lastPacketMs(&file, blocks.constLast()) : 0;
        out->end = out->start.addMSecs(out->durationMs);
    }

    // Tiêu đề theo tên file như step-07; hậu tố _2, _3… (ghi lại trong cùng một
    // giây) giữ trong ngoặc để hai file không trùng tên hiển thị.
    const QString base = QFileInfo(path).completeBaseName();
    static const QRegularExpression re(
        QStringLiteral("^(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})(?:_(\\d+))?$"));
    const QRegularExpressionMatch m = re.match(base);
    if (m.hasMatch()) {
        out->title = QStringLiteral("%1/%2/%3 %4:%5:%6")
                         .arg(m.captured(1), m.captured(2), m.captured(3), m.captured(4), m.captured(5),
                              m.captured(6));
        if (!m.captured(7).isEmpty())
            out->title += QStringLiteral(" (%1)").arg(m.captured(7));
    } else {
        out->title = base;
    }
    return true;
}

QVector<Summary> scanRecords(const QString &root, QStringList *errors)
{
    QVector<Summary> list;
    const QDir rootDir(root);
    QDirIterator it(root, {QStringLiteral("*.rec")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        Summary s;
        QString error;
        if (!summarize(path, &s, &error)) {
            if (errors)
                errors->append(QStringLiteral("./records/%1: %2").arg(rootDir.relativeFilePath(path), error));
            continue;
        }
        s.relName = rootDir.relativeFilePath(path);
        list.append(s);
    }
    // Tên file theo giờ bắt đầu nên so tên là so thời gian; cũ nhất về cuối.
    std::sort(list.begin(), list.end(), [](const Summary &a, const Summary &b) {
        return QFileInfo(a.path).fileName() > QFileInfo(b.path).fileName();
    });
    return list;
}

} // namespace RecordFormat
