#include "proto/ScnCf.h"

#include <QStringList>

namespace ScnCf {

namespace {

quint16 u16(const uchar *p) { return quint16(p[0] | (p[1] << 8)); }
quint32 u32(const uchar *p) { return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24); }
quint64 u64(const uchar *p) { return quint64(u32(p)) | (quint64(u32(p + 4)) << 32); }

void put16(QByteArray *b, quint16 v)
{
    b->append(char(v & 0xff));
    b->append(char(v >> 8));
}

void put32(QByteArray *b, quint32 v)
{
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

void putChunk(QByteArray *b, quint8 code, quint8 len)
{
    b->append(char(code));
    b->append(char(len));
}

const char *const kTypeNames[MsgTypeCount] = {
    "None", "WakeUp", "Shutdown", "Status", "Info", "Command", "Query", "Reply", "Mark",
    "Line", "Position", "Time", "Plot", "Track", "Jammer", "Video", "KeepAlive",
    "JamLevel", "Sector", "JamStrobe",
};

} // namespace

bool decode(const QByteArray &raw, Message *out, QString *error)
{
    *out = Message();
    const auto *p = reinterpret_cast<const uchar *>(raw.constData());
    const int n = int(raw.size());
    if (n < kHeadBytes) {
        if (error)
            *error = QStringLiteral("gói %1 byte, ngắn hơn đầu gói").arg(n);
        return false;
    }
    // SW1 không kiểm tra Header; ở đây cũng chỉ ghi nhận chứ không loại gói, vì
    // chưa có gói bắt thật để biết Header có khi nào khác 04 03 không.
    out->length = u16(p + 2);
    out->type = u16(p + 4);

    // SW1 bỏ qua Length và tách chunk tới hết datagram; làm giống vậy nhưng
    // kiểm tra biên từng chunk (SW1 đọc vượt cuối gói là chết luồng nhận).
    int pos = kHeadBytes;
    while (pos < n) {
        if (pos + 2 > n) {
            if (error)
                *error = QStringLiteral("đầu chunk cụt ở byte %1").arg(pos);
            return false;
        }
        const quint8 code = p[pos];
        const int len = p[pos + 1];
        const uchar *d = p + pos + 2;
        if (pos + 2 + len > n) {
            if (error)
                *error = QStringLiteral("chunk 0x%1 dài %2 byte vượt cuối gói")
                             .arg(code, 2, 16, QLatin1Char('0')).arg(len);
            return false;
        }
        // Chunk có cỡ khác đặc tả thì coi như chunk lạ chứ không đọc sai lệch.
        const auto need = [len, out](int size) {
            if (len == size)
                return true;
            ++out->unknownChunks;
            return false;
        };

        switch (code) {
        case ChNew:    out->isNew = true; break;
        case ChUpdate: out->isUpdate = true; break;
        case ChSet:    out->isSet = true; break;
        case ChDelete: out->isDelete = true; break;
        case ChNorth:  out->north = true; break;
        case ChTrackId:
            if (need(4)) out->trackId = u32(d);
            break;
        case ChNumber:
            if (need(2)) out->number = u16(d);
            break;
        case ChPositionXy:
            if (need(8)) {
                out->x = qint32(u32(d));
                out->y = qint32(u32(d + 4));
            }
            break;
        case ChPositionRAlpha:
            if (need(8)) {
                out->rangeM = u32(d);
                out->alphaMas = u32(d + 4);
            }
            break;
        case ChPositionAlpha:
            if (need(4)) out->positionAlphaMas = qint32(u32(d));
            break;
        case ChVelocityXy:
            if (need(4)) {
                out->vx = qint16(u16(d));
                out->vy = qint16(u16(d + 2));
            }
            break;
        case ChTrackQuality:
            if (need(1)) out->trackQuality = d[0];
            break;
        case ChTargetSource:
            if (need(2)) out->targetSource = u16(d);
            break;
        case ChPsrChannels:
            if (need(1)) out->psrChannels = d[0];
            break;
        case ChTransmitMode:
            if (need(3)) out->transmitMode = QByteArray(reinterpret_cast<const char *>(d), 3);
            break;
        case ChTimeStampWide:
            // u64 giây + u32 nano giây. SW1 đọc giây 4 byte, nano giây lệch offset.
            if (need(12)) out->time = TimeStamp{u64(d), u32(d + 8)};
            break;
        case ChIffNrz:
            if (need(2)) out->iffNrz = u16(d);
            break;
        case ChIffNrzDevice:
            if (need(1)) out->nrzDevice = d[0];
            break;
        case ChIffNrzMode:
            if (need(1)) out->nrzMode = d[0];
            break;
        default:
            ++out->unknownChunks;
            break;
        }
        pos += 2 + len;
    }
    return true;
}

QByteArray encodePlot(const PlotOut &plot)
{
    QByteArray b;
    b.reserve(38);
    b.append(char(kHeader0));
    b.append(char(kHeader1));
    put16(&b, 0);                  // Length, điền khi đã biết cỡ
    put16(&b, MsgPlot);

    putChunk(&b, ChTimeStampWide, 12);
    put32(&b, quint32(plot.time.sec & 0xffffffffu));
    put32(&b, quint32(plot.time.sec >> 32));
    put32(&b, plot.time.nsec);

    putChunk(&b, ChPositionRAlpha, 8);
    put32(&b, plot.rangeM);
    put32(&b, plot.alphaMas);

    putChunk(&b, ChTargetSource, 2);
    put16(&b, plot.targetSource);

    putChunk(&b, ChIffNrz, 2);
    put16(&b, plot.iffNrz);

    const quint16 len = quint16(b.size());
    b[2] = char(len & 0xff);
    b[3] = char(len >> 8);
    return b;
}

QByteArray helloBytes()
{
    return QByteArrayLiteral("0");
}

QString typeName(quint16 type)
{
    return type < MsgTypeCount ? QString::fromLatin1(kTypeNames[type])
                               : QStringLiteral("loại %1").arg(type);
}

QString describe(const Message &m)
{
    QStringList parts;
    parts << typeName(m.type);
    if (m.trackId)
        parts << QStringLiteral("TrackId %1").arg(*m.trackId);
    if (m.number)
        parts << QStringLiteral("Number %1").arg(*m.number);
    if (m.rangeM && m.alphaMas) {
        parts << QStringLiteral("%1° - %2km")
                     .arg(QString::number(*m.alphaMas / kMasPerDegree, 'f', 3),
                          QString::number(*m.rangeM / 1000.0, 'f', 3));
    }
    if (m.x && m.y)
        parts << QStringLiteral("x %1 m, y %2 m").arg(*m.x).arg(*m.y);
    if (m.vx && m.vy)
        parts << QStringLiteral("vx %1, vy %2 m/s").arg(*m.vx).arg(*m.vy);
    if (m.targetSource)
        parts << QStringLiteral("nguồn %1").arg(*m.targetSource);
    if (m.iffNrz)
        parts << QStringLiteral("NRZ 0x%1").arg(*m.iffNrz, 4, 16, QLatin1Char('0'));
    if (m.isDelete)
        parts << QStringLiteral("xoá");
    if (m.unknownChunks > 0)
        parts << QStringLiteral("%1 chunk lạ").arg(m.unknownChunks);
    return parts.join(QStringLiteral(", "));
}

} // namespace ScnCf
