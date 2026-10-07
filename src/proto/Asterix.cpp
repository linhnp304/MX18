#include "proto/Asterix.h"

#include <QStringList>
#include <QTime>
#include <QtMath>

#include <cmath>
#include <initializer_list>

namespace Asterix {

namespace {

constexpr double kNm = 1852.0;
constexpr double kFt = 0.3048;
constexpr int kMaxFspecBytes = 5;          // CAT048 có 28 FRN = 4 byte; dư một byte

quint32 u16(const uchar *p) { return (quint32(p[0]) << 8) | p[1]; }
quint32 u24(const uchar *p) { return (quint32(p[0]) << 16) | (quint32(p[1]) << 8) | p[2]; }
qint32 s16(const uchar *p) { return qint16(quint16(u16(p))); }
qint32 s24(const uchar *p)
{
    const quint32 v = u24(p);
    return (v & 0x800000u) ? qint32(v) - 0x1000000 : qint32(v);
}
// Số có dấu 14 bit nằm ở bit 14..1 của hai byte (I048/090, I048/110).
qint32 s14(const uchar *p)
{
    const quint32 v = u16(p) & 0x3fffu;
    return (v & 0x2000u) ? qint32(v) - 0x4000 : qint32(v);
}

double angle16(const uchar *p) { return u16(p) * 360.0 / 65536.0; }

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    return false;
}

// ----------------------------------------------------------- độ dài item

// Item mở rộng (FX ở bit 1 mỗi byte): -1 khi chạy quá cuối khối.
int extendedLen(const uchar *q, const uchar *end)
{
    int n = 0;
    do {
        if (q + n >= end)
            return -1;
        ++n;
    } while (q[n - 1] & 0x01);
    return n;
}

int repetitiveLen(const uchar *q, const uchar *end, int size)
{
    return q < end ? 1 + q[0] * size : -1;
}

int explicitLen(const uchar *q, const uchar *end)
{
    return (q < end && q[0] >= 1) ? q[0] : -1;
}

// Item ghép: byte chính (có FX) báo subfield nào có mặt; sizes[i] là cỡ của
// subfield ứng với bit 8 - i của byte chính đầu tiên (0 = bit không dùng).
int compoundLen(const uchar *q, const uchar *end, const int (&sizes)[7])
{
    const int primary = extendedLen(q, end);
    if (primary < 0)
        return -1;
    int n = primary;
    for (int i = 0; i < 7; ++i) {
        if (q[0] & (0x80 >> i))
            n += sizes[i];
    }
    return n;
}

// SP kiểu ELM-2288 của SCH-VQ: RM · cờ · các khối theo cờ.
int parolLen(const uchar *q, const uchar *end)
{
    if (end - q < 2)
        return -1;
    const quint8 f = q[1];
    int n = 2;
    if (f & 0x40) n += 4;   // POR
    if (f & 0x20) n += 3;   // TIME
    if (f & 0x08) n += 1;   // F
    if (f & 0x04) n += 2;   // H
    if (f & 0x02) n += 3;   // ID
    return n;
}

int itemLen048(int frn, const uchar *q, const uchar *end, const DecodeOptions &opt)
{
    switch (frn) {
    case 1: case 5: case 6: case 11: case 17: case 19: case 21: case 24: case 26: return 2;
    case 2: case 8: return 3;
    case 4: case 12: case 13: case 15: case 18: return 4;
    case 9: return 6;
    case 22: return 7;
    case 23: case 25: return 1;
    case 3: case 14: case 16: return extendedLen(q, end);
    case 10: return repetitiveLen(q, end, 8);
    case 7: {
        static const int kSizes[7] = {1, 1, 1, 1, 1, 1, 1};   // SRL SRR SAM PRL PAM RPD APD
        return compoundLen(q, end, kSizes);
    }
    case 20: {
        // I048/120: CAL 2 byte, RDS lặp 6 byte (SW0 giả định REP = 1).
        const int primary = extendedLen(q, end);
        if (primary < 0)
            return -1;
        int n = primary;
        if (q[0] & 0x80)
            n += 2;
        if (q[0] & 0x40) {
            if (q + n >= end)
                return -1;
            n += 1 + q[n] * 6;
        }
        return n;
    }
    case 27: return opt.parolSp ? parolLen(q, end) : explicitLen(q, end);
    case 28: return explicitLen(q, end);
    default: return -1;
    }
}

int itemLen034(int frn, const uchar *q, const uchar *end)
{
    switch (frn) {
    case 1: case 5: case 12: return 2;
    case 2: case 4: case 10: return 1;
    case 3: return 3;
    case 9: case 11: return 8;
    case 6: {
        static const int kSizes[7] = {1, 0, 0, 1, 1, 2, 0};   // COM · · PSR SSR MDS
        return compoundLen(q, end, kSizes);
    }
    case 7: {
        static const int kSizes[7] = {1, 0, 0, 1, 1, 1, 0};
        return compoundLen(q, end, kSizes);
    }
    case 8: return repetitiveLen(q, end, 2);
    case 13: case 14: return explicitLen(q, end);
    default: return -1;
    }
}

// ----------------------------------------------------------- mở item

void readSpNrz(const uchar *q, int len, Cat048 *r)
{
    // Dạng P18M: LEN = 4 · FSPEC con 0x80 (có khối NRZ) · NRZ1 · NRZ2.
    if (len >= 4 && (q[1] & 0x80)) {
        r->nrz1 = q[2];
        r->nrz2 = q[3];
    }
}

void readParol(const uchar *q, Cat048 *r)
{
    ParolIff p;
    p.rm = q[0] & 0x0f;
    const quint8 f = q[1];
    p.friendly = f & 0x80;
    p.commander = f & 0x10;
    int n = 2;
    if (f & 0x40) {
        p.rangeM = u16(q + n) * kNm / 256.0;
        p.azimuthDeg = angle16(q + n + 2);
        n += 4;
    }
    if (f & 0x20)
        n += 3;
    if (f & 0x08)
        p.fuel = q[n++];
    if (f & 0x04) {
        p.heightM = s14(q + n) * 25.0 * kFt;
        n += 2;
    }
    if (f & 0x02)
        p.id = ((quint32(q[n]) & 0x01) << 16) | (quint32(q[n + 1]) << 8) | q[n + 2];
    r->parol = p;
}

void readItem048(int frn, const uchar *q, int len, const DecodeOptions &opt, Cat048 *r)
{
    switch (frn) {
    case 1: r->sac = q[0]; r->sic = q[1]; break;
    case 2: r->timeOfDay = u24(q) / 128.0; break;
    case 3:
        r->typ = (q[0] >> 5) & 0x07;
        r->sim = q[0] & 0x10;
        r->rdp = q[0] & 0x08;
        r->spi = q[0] & 0x04;
        r->rab = q[0] & 0x02;
        if (len >= 2) {
            r->tst = q[1] & 0x80;
            r->me = q[1] & 0x10;
            r->mi = q[1] & 0x08;
            r->foeFri = (q[1] >> 1) & 0x03;
        }
        break;
    case 4:
        r->rangeM = u16(q) * kNm / 256.0;
        r->azimuthDeg = angle16(q + 2);
        break;
    case 5:
        r->mode3aV = q[0] & 0x80;
        r->mode3aG = q[0] & 0x40;
        r->mode3aL = q[0] & 0x20;
        r->mode3a = quint16(u16(q) & 0x0fffu);
        break;
    case 6: r->flightLevel = s14(q) / 4.0; break;
    case 8: r->aircraftAddress = u24(q); break;
    case 11: r->trackNumber = quint16(u16(q) & 0x0fffu); break;
    case 12:
        r->x = s16(q) * kNm / 128.0;
        r->y = s16(q + 2) * kNm / 128.0;
        break;
    case 13:
        r->speed = u16(q) * kNm / 16384.0;
        r->heading = angle16(q + 2);
        break;
    case 14:
        r->hasStatus = true;
        r->cnf = q[0] & 0x80;
        r->rad = (q[0] >> 5) & 0x03;
        r->dou = q[0] & 0x10;
        r->mah = q[0] & 0x08;
        r->cdm = (q[0] >> 1) & 0x03;
        if (len >= 2) {
            r->tre = q[1] & 0x80;
            r->gho = q[1] & 0x40;
            r->sup = q[1] & 0x20;
            r->tcc = q[1] & 0x10;
        }
        break;
    case 19: r->heightM = s14(q) * 25.0 * kFt; break;
    case 27:
        if (opt.parolSp)
            readParol(q, r);
        else
            readSpNrz(q, len, r);
        break;
    case 28:
        // SW0 đọc NRZ ở FRN28 (lệch chuẩn); chưa có gói bắt thật nên đọc cả hai chỗ.
        readSpNrz(q, len, r);
        break;
    default:
        break;   // item có độ dài nhưng MX18 không dùng
    }
}

void readItem034(int frn, const uchar *q, int len, Cat034 *m)
{
    switch (frn) {
    case 1: m->sac = q[0]; m->sic = q[1]; break;
    case 2: m->messageType = q[0]; break;
    case 3: m->timeOfDay = u24(q) / 128.0; break;
    case 4: m->sector = q[0]; break;
    case 5: m->rotationPeriod = u16(q) / 128.0; break;
    case 11:
        m->siteHeight = s16(q);
        m->siteLat = s24(q + 2) * 180.0 / 8388608.0;
        m->siteLon = s24(q + 5) * 180.0 / 8388608.0;
        break;
    case 14: {
        // SP của P18M: LEN · FSPEC con (bit 8 = P18C, bit 7 = NRZ) · các byte con.
        if (len < 2)
            break;
        int n = 2;
        if ((q[1] & 0x80) && n < len)
            m->p18c = q[n++];
        if ((q[1] & 0x40) && n < len)
            m->nrz = q[n];
        break;
    }
    default:
        break;
    }
}

// Một bản ghi bắt đầu ở *pos; đọc xong thì *pos trỏ sang bản ghi kế tiếp.
bool decodeRecord(quint8 cat, const uchar **pos, const uchar *end, const DecodeOptions &opt,
                  Batch *out, QString *error)
{
    const uchar *q = *pos;
    bool present[kMaxFspecBytes * 7 + 1] = {false};
    int fspecBytes = 0;
    uchar f = 0;
    do {
        if (q >= end)
            return fail(error, QStringLiteral("FSPEC cụt"));
        if (fspecBytes == kMaxFspecBytes)
            return fail(error, QStringLiteral("FSPEC dài quá %1 byte").arg(kMaxFspecBytes));
        f = *q++;
        for (int bit = 0; bit < 7; ++bit) {
            if (f & (0x80 >> bit))
                present[fspecBytes * 7 + bit + 1] = true;
        }
        ++fspecBytes;
    } while (f & 0x01);

    Cat034 svc;
    Cat048 rep;
    for (int frn = 1; frn <= fspecBytes * 7; ++frn) {
        if (!present[frn])
            continue;
        const int len = (cat == kCat048) ? itemLen048(frn, q, end, opt) : itemLen034(frn, q, end);
        if (len < 0 || q + len > end) {
            return fail(error, QStringLiteral("CAT%1 FRN%2 %3")
                                   .arg(cat, 3, 10, QLatin1Char('0')).arg(frn)
                                   .arg(len < 0 && frn > (cat == kCat048 ? 28 : 14)
                                            ? QStringLiteral("không có trong UAP")
                                            : QStringLiteral("vượt cuối khối")));
        }
        if (cat == kCat048)
            readItem048(frn, q, len, opt, &rep);
        else
            readItem034(frn, q, len, &svc);
        q += len;
    }

    if (cat == kCat048)
        out->reports.append(rep);
    else
        out->services.append(svc);
    *pos = q;
    return true;
}

// ----------------------------------------------------------- ghi

class Writer
{
public:
    explicit Writer(quint8 cat) { m_b.append(char(cat)); m_b.append(2, '\0'); }

    void put8(quint32 v) { m_b.append(char(v & 0xff)); }
    void put16(quint32 v) { put8(v >> 8); put8(v); }
    void put24(quint32 v) { put8(v >> 16); put8(v >> 8); put8(v); }

    // FSPEC từ danh sách FRN có mặt (tăng dần): mỗi byte 7 cờ + FX.
    void fspec(std::initializer_list<int> frns)
    {
        int last = 0;
        for (int frn : frns)
            last = qMax(last, frn);
        const int n = (last + 6) / 7;
        QByteArray f(n, '\0');
        for (int frn : frns)
            f[(frn - 1) / 7] = char(f.at((frn - 1) / 7) | (0x80 >> ((frn - 1) % 7)));
        for (int i = 0; i + 1 < n; ++i)
            f[i] = char(f.at(i) | 0x01);
        m_b.append(f);
    }

    QByteArray finish()
    {
        const int len = int(m_b.size());
        m_b[1] = char((len >> 8) & 0xff);
        m_b[2] = char(len & 0xff);
        return m_b;
    }

private:
    QByteArray m_b;
};

// Phép ép kiểu của SW0 cắt phần lẻ về phía 0; giữ y như vậy để các vector hex
// của file 03 khớp từng byte, chỉ thêm kẹp dải thay vì để tràn số.
quint32 truncU16(double v)
{
    return quint32(qBound(0.0, std::trunc(v), 65535.0));
}

// Số có dấu 16 bit theo bù hai chuẩn (SW0 cộng 0xFFFF nên lệch 1 LSB khi âm).
quint32 truncS16(double v)
{
    return quint32(qint32(qBound(-32768.0, std::trunc(v), 32767.0))) & 0xffffu;
}

quint32 angleU16(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0)
        d += 360.0;
    return truncU16(d * 65536.0 / 360.0);
}

// I048/110 và độ cao trong SP PAROL: 14 bit có dấu, LSB 25 ft.
quint32 height14(double m)
{
    const double v = qBound(-8192.0, std::trunc(m / kFt / 25.0), 8191.0);
    return quint32(qint32(v)) & 0x3fffu;
}

quint32 latLon24(double deg)
{
    return quint32(qint32(std::trunc(deg * 8388608.0 / 180.0))) & 0xffffffu;
}

void putParol(Writer *w, const TargetOut &t)
{
    // Kiểu ELM-2288 "PAROL" (file 03 mục 6.2): luôn nhận dạng ta, luôn có vị
    // trí và số hiệu; bit 1 SW0 luôn bật.
    const bool fuel = t.fuel > 0;
    const bool height = t.heightM > 0.0;
    quint8 flags = 0x80 | 0x40 | 0x02 | 0x01;
    if (t.commander) flags |= 0x10;
    if (fuel) flags |= 0x08;
    if (height) flags |= 0x04;

    w->put8(t.returnedMode & 0x0f);
    w->put8(flags);
    // Cự ly trong SP luôn 1/256 NM, không nhân hệ số k như I048/040.
    w->put16(truncU16(t.rangeM * 256.0 / kNm));
    w->put16(angleU16(t.azimuthDeg));
    if (fuel)
        w->put8(t.fuel);
    if (height)
        w->put16(height14(t.heightM));
    w->put8((t.flightId >> 16) & 0x01);
    w->put8(t.flightId >> 8);
    w->put8(t.flightId);
}

} // namespace

// ------------------------------------------------------------------ giải mã

bool decode(const QByteArray &datagram, Batch *out, QString *error, const DecodeOptions &options)
{
    *out = Batch();
    const auto *base = reinterpret_cast<const uchar *>(datagram.constData());
    const int n = int(datagram.size());
    int pos = 0;
    while (pos < n) {
        if (n - pos < 3)
            return fail(error, QStringLiteral("đầu khối cụt ở byte %1").arg(pos));
        const quint8 cat = base[pos];
        const int len = int(u16(base + pos + 1));
        if (len < 3 || pos + len > n) {
            return fail(error, QStringLiteral("khối CAT%1 ở byte %2 ghi LEN %3, datagram còn %4 byte")
                                   .arg(cat).arg(pos).arg(len).arg(n - pos));
        }
        if (cat == kCat034 || cat == kCat048) {
            const uchar *q = base + pos + 3;
            const uchar *end = base + pos + len;
            while (q < end) {
                QString why;
                if (!decodeRecord(cat, &q, end, options, out, &why)) {
                    return fail(error, QStringLiteral("khối CAT%1 ở byte %2: %3")
                                           .arg(cat).arg(pos).arg(why));
                }
            }
        } else {
            ++out->otherBlocks;
        }
        pos += len;
    }
    return true;
}

// ------------------------------------------------------------------ mã hoá

quint32 timeOfDay(const QDateTime &utc)
{
    const qint64 ms = utc.toUTC().time().msecsSinceStartOfDay();
    return quint32(ms * 128 / 1000) & 0xffffffu;
}

quint32 timeOfDayNow()
{
    return timeOfDay(QDateTime::currentDateTimeUtc());
}

QByteArray northMarker(const EncodeConfig &cfg, quint32 tod, double periodS, const Site &site)
{
    Writer w(kCat034);
    w.fspec({1, 2, 3, 5, 11});
    w.put8(cfg.sac);
    w.put8(cfg.sic);
    w.put8(Cat034::NorthMarker);
    w.put24(tod);
    w.put16(truncU16(periodS * 128.0));
    w.put16(quint32(qint32(qBound(-32768, site.heightM, 32767))) & 0xffffu);
    w.put24(latLon24(site.lat));
    w.put24(latLon24(site.lon));
    return w.finish();
}

QByteArray sectorCrossing(const EncodeConfig &cfg, quint32 tod, quint8 sector)
{
    Writer w(kCat034);
    w.fspec({1, 2, 3, 4});
    w.put8(cfg.sac);
    w.put8(cfg.sic);
    w.put8(Cat034::SectorCrossing);
    w.put24(tod);
    w.put8(sector);
    return w.finish();
}

QByteArray jammingStrobe(const EncodeConfig &cfg, quint32 tod, double azimuthDeg)
{
    // Cửa sổ cực I034/100 phủ cả cự ly, góc đầu = góc cuối = hướng nhiễu.
    Writer w(kCat034);
    w.fspec({1, 2, 3, 9});
    w.put8(cfg.sac);
    w.put8(cfg.sic);
    w.put8(Cat034::JammingStrobe);
    w.put24(tod);
    w.put16(0);
    w.put16(0xffff);
    const quint32 theta = angleU16(azimuthDeg);
    w.put16(theta);
    w.put16(theta);
    return w.finish();
}

QByteArray targetReport(const EncodeConfig &cfg, quint32 tod, const TargetOut &t)
{
    Writer w(kCat048);
    if (t.isTrack) {
        if (t.iff)
            w.fspec({1, 2, 3, 4, 11, 12, 13, 14, 19, 27});
        else
            w.fspec({1, 2, 3, 4, 11, 12, 13, 14, 19});
    } else {
        if (t.iff)
            w.fspec({1, 2, 3, 4, 14, 19, 27});
        else
            w.fspec({1, 2, 3, 4, 14, 19});
    }

    w.put8(cfg.sac);
    w.put8(cfg.sic);
    w.put24(tod);

    // I048/020 (file 03 mục 5.1): quỹ đạo/điểm dấu radar là PSR, FOE/FRI chưa
    // biết; gói IFF là bạn, điểm dấu IFF là SSR có MI.
    if (!t.iff) {
        w.put8(0x21);
        w.put8(0x04);
    } else if (t.isTrack) {
        w.put8(0x21);
        w.put8(0x02);
    } else {
        w.put8(0x41);
        w.put8(0x0a);
    }

    w.put16(truncU16(t.rangeM * 128.0 * cfg.rangeChange / kNm));
    w.put16(angleU16(t.azimuthDeg));

    if (t.isTrack) {
        w.put16(t.trackNumber & 0x0fffu);
        // X/Y tính lại từ toạ độ cực như SW0, cùng hệ số k với cự ly.
        const double a = t.azimuthDeg * M_PI / 180.0;
        const double scale = 64.0 * cfg.rangeChange / kNm;
        w.put16(truncS16(t.rangeM * std::sin(a) * scale));
        w.put16(truncS16(t.rangeM * std::cos(a) * scale));
        w.put16(truncU16(t.speedMps * 16384.0 / kNm));
        w.put16(angleU16(t.headingDeg));
        // I048/170: xác nhận, PSR; byte hai mang TRE ở bản tin cuối.
        w.put8(0x21);
        w.put8(t.endOfTrack ? 0x80 : 0x00);
    } else {
        // Điểm dấu: tentative + ghost (đúng như SW0, để VQ không lập quỹ đạo).
        w.put8(t.iff ? 0xc1 : 0xa1);
        w.put8(0x40);
    }
    w.put16(height14(t.heightM));

    if (t.iff) {
        if (cfg.outputP18m) {
            w.put8(4);
            w.put8(0x80);
            w.put8(t.nrz1);
            w.put8(t.nrz2);
        } else {
            putParol(&w, t);
        }
    }
    return w.finish();
}

// ------------------------------------------------------------------ mô tả

QString describe(const Cat034 &m)
{
    static const char *const kNames[] = {"?", "North marker", "Sector crossing",
                                         "Geographical filtering", "Jamming strobe"};
    QStringList parts;
    parts << QString::fromLatin1(m.messageType <= 4 ? kNames[m.messageType] : kNames[0]);
    parts << QStringLiteral("SAC/SIC %1/%2").arg(m.sac).arg(m.sic);
    if (m.sector)
        parts << QStringLiteral("sector %1 (%2°)").arg(*m.sector).arg(*m.sector * 360.0 / 256.0, 0, 'f', 2);
    if (m.rotationPeriod)
        parts << QStringLiteral("chu kỳ %1 s").arg(*m.rotationPeriod, 0, 'f', 2);
    if (m.siteLat && m.siteLon)
        parts << QStringLiteral("đài %1, %2, %3 m").arg(*m.siteLat, 0, 'f', 5)
                     .arg(*m.siteLon, 0, 'f', 5).arg(m.siteHeight.value_or(0));
    if (m.timeOfDay)
        parts << QTime::fromMSecsSinceStartOfDay(int(*m.timeOfDay * 1000.0)).toString(QStringLiteral("HH:mm:ss.zzz"));
    return parts.join(QStringLiteral(", "));
}

QString describe(const Cat048 &m)
{
    QStringList parts;
    parts << (m.trackNumber ? QStringLiteral("Quỹ đạo TN %1").arg(*m.trackNumber)
                            : QStringLiteral("Điểm dấu"));
    parts << QStringLiteral("SAC/SIC %1/%2").arg(m.sac).arg(m.sic);
    if (m.rangeM && m.azimuthDeg)
        parts << QStringLiteral("%1° - %2km").arg(*m.azimuthDeg, 0, 'f', 3).arg(*m.rangeM / 1000.0, 0, 'f', 3);
    if (m.x && m.y)
        parts << QStringLiteral("x %1 y %2 km").arg(*m.x / 1000.0, 0, 'f', 3).arg(*m.y / 1000.0, 0, 'f', 3);
    if (m.speed && m.heading)
        parts << QStringLiteral("%1 m/s hướng %2°").arg(*m.speed, 0, 'f', 1).arg(*m.heading, 0, 'f', 2);
    if (m.heightM)
        parts << QStringLiteral("cao %1 m").arg(*m.heightM, 0, 'f', 0);
    if (m.mode3a)
        parts << QStringLiteral("3/A %1").arg(*m.mode3a, 4, 8, QLatin1Char('0'));
    if (m.nrz1 && m.nrz2)
        parts << QStringLiteral("NRZ %1 %2").arg(*m.nrz1, 2, 16, QLatin1Char('0')).arg(*m.nrz2, 2, 16, QLatin1Char('0'));
    if (m.parol) {
        const ParolIff &p = *m.parol;
        QString s = QStringLiteral("PAROL RM %1").arg(p.rm);
        if (p.commander) s += QStringLiteral(" chỉ huy");
        if (p.fuel) s += QStringLiteral(" nhiên liệu %1%").arg(*p.fuel);
        if (p.heightM) s += QStringLiteral(" cao %1 m").arg(*p.heightM, 0, 'f', 0);
        if (p.id) s += QStringLiteral(" số hiệu %1").arg(*p.id);
        if (p.rangeM && p.azimuthDeg)
            s += QStringLiteral(" (%1° - %2km)").arg(*p.azimuthDeg, 0, 'f', 3).arg(*p.rangeM / 1000.0, 0, 'f', 3);
        parts << s;
    }
    if (m.tre)
        parts << QStringLiteral("TRE");
    if (m.gho)
        parts << QStringLiteral("ghost");
    return parts.join(QStringLiteral(", "));
}

} // namespace Asterix
