#include "map/DbfReader.h"

#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

// Bảng 0x80..0xFF của CP1250 (Trung Âu). Ô bỏ trống trong bảng gốc thay bằng
// U+FFFD để không sinh ký tự lạ.
const ushort kCp1250High[128] = {
    0x20AC, 0xFFFD, 0x201A, 0xFFFD, 0x201E, 0x2026, 0x2020, 0x2021,
    0xFFFD, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

} // namespace

bool DbfReader::open(const QString &dbfPath)
{
    m_fieldNames.clear();
    m_records.clear();

    QFile f(dbfPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();
    if (data.size() < 33)
        return false;

    // Bảng mã lấy từ .cpg cùng tên.
    const QString cpg = QFileInfo(dbfPath).path() + QLatin1Char('/')
                        + QFileInfo(dbfPath).completeBaseName() + QStringLiteral(".cpg");
    QFile cf(cpg);
    if (cf.open(QIODevice::ReadOnly)) {
        const QString tag = QString::fromLatin1(cf.readAll()).trimmed().toUpper();
        if (tag.contains(QStringLiteral("UTF-8")) || tag.contains(QStringLiteral("UTF8"))
            || tag.contains(QStringLiteral("65001")))
            m_codec = Utf8;
        else if (tag.contains(QStringLiteral("1250")))
            m_codec = Cp1250;
        cf.close();
    }

    const quint8 *b = reinterpret_cast<const quint8 *>(data.constData());
    const qint32 numRecords = qint32(quint32(b[4]) | (quint32(b[5]) << 8)
                                     | (quint32(b[6]) << 16) | (quint32(b[7]) << 24));
    const int headerLen = int(quint32(b[8]) | (quint32(b[9]) << 8));
    const int recordLen = int(quint32(b[10]) | (quint32(b[11]) << 8));
    if (headerLen < 33 || recordLen <= 0 || numRecords < 0)
        return false;

    QVector<int> widths;
    for (int off = 32; off + 32 <= headerLen - 1 && b[off] != 0x0D; off += 32) {
        QByteArray name(reinterpret_cast<const char *>(b + off), 11);
        const int nul = name.indexOf('\0');
        if (nul >= 0)
            name.truncate(nul);
        m_fieldNames << QString::fromLatin1(name).trimmed();
        widths << int(b[off + 16]);
    }
    if (widths.isEmpty())
        return false;

    m_records.reserve(numRecords);
    for (qint32 r = 0; r < numRecords; ++r) {
        const qint64 base = qint64(headerLen) + qint64(r) * recordLen;
        if (base + recordLen > data.size())
            break;
        if (data.at(base) == '*') // bản ghi đã xoá
            continue;
        QVector<QByteArray> rec;
        rec.reserve(widths.size());
        qint64 off = base + 1;
        for (int w : std::as_const(widths)) {
            rec.append(data.mid(off, w).trimmed());
            off += w;
        }
        m_records.append(rec);
    }
    return true;
}

int DbfReader::fieldIndex(const QString &name) const
{
    for (int i = 0; i < m_fieldNames.size(); ++i) {
        if (m_fieldNames.at(i).compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString DbfReader::decode(const QByteArray &raw) const
{
    switch (m_codec) {
    case Utf8: {
        QStringDecoder dec(QStringDecoder::Utf8);
        return dec(raw);
    }
    case Cp1250: {
        QString out;
        out.reserve(raw.size());
        for (char ch : raw) {
            const quint8 u = quint8(ch);
            out.append(u < 0x80 ? QChar(u) : QChar(kCp1250High[u - 0x80]));
        }
        return out;
    }
    case Latin1:
    default:
        return QString::fromLatin1(raw);
    }
}

QString DbfReader::value(int record, int field) const
{
    if (record < 0 || record >= m_records.size())
        return QString();
    const QVector<QByteArray> &rec = m_records.at(record);
    if (field < 0 || field >= rec.size())
        return QString();
    return decode(rec.at(field));
}

QString DbfReader::value(int record, const QString &field) const
{
    return value(record, fieldIndex(field));
}
