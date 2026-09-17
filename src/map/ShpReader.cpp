#include "map/ShpReader.h"

#include <QFile>

#include <cstring>

namespace {

// Shapefile trộn hai thứ tự byte: header bản ghi big-endian, nội dung little-endian.
inline qint32 beInt(const char *p)
{
    const quint8 *b = reinterpret_cast<const quint8 *>(p);
    return qint32((quint32(b[0]) << 24) | (quint32(b[1]) << 16) | (quint32(b[2]) << 8) | quint32(b[3]));
}

inline qint32 leInt(const char *p)
{
    const quint8 *b = reinterpret_cast<const quint8 *>(p);
    return qint32(quint32(b[0]) | (quint32(b[1]) << 8) | (quint32(b[2]) << 16) | (quint32(b[3]) << 24));
}

inline double leDouble(const char *p)
{
    double d;
    std::memcpy(&d, p, sizeof(double));
    return d; // mọi nền tảng đích đều little-endian
}

} // namespace

namespace ShpReader {

bool read(const QString &shpPath, QVector<Feature> *out, int *shapeTypeOut)
{
    QFile f(shpPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();

    if (data.size() < 100)
        return false;
    if (shapeTypeOut)
        *shapeTypeOut = leInt(data.constData() + 32);

    const char *base = data.constData();
    qint64 off = 100;
    const qint64 size = data.size();

    while (off + 8 <= size) {
        const qint32 contentWords = beInt(base + off + 4);
        const qint64 contentBytes = qint64(contentWords) * 2;
        off += 8;
        if (contentBytes <= 0 || off + contentBytes > size)
            break;

        const char *rec = base + off;
        // Biến thể Z (11/13/15/18) và M (21/23/25/28) có phần toạ độ XY nằm
        // đúng chỗ như kiểu gốc, nên quy về kiểu gốc bằng phép chia dư 10.
        const qint32 type = leInt(rec) % 10;

        Feature feat;
        switch (type) {
        case Point: {
            if (contentBytes >= 20)
                feat.parts.append({QPointF(leDouble(rec + 4), leDouble(rec + 12))});
            break;
        }
        case MultiPoint: {
            if (contentBytes >= 40) {
                const qint32 n = leInt(rec + 36);
                QVector<QPointF> pts;
                pts.reserve(n);
                for (qint32 i = 0; i < n && 40 + qint64(i) * 16 + 16 <= contentBytes; ++i)
                    pts.append(QPointF(leDouble(rec + 40 + i * 16), leDouble(rec + 48 + i * 16)));
                if (!pts.isEmpty())
                    feat.parts.append(pts);
            }
            break;
        }
        case PolyLine:
        case Polygon: {
            if (contentBytes < 44)
                break;
            const qint32 numParts = leInt(rec + 36);
            const qint32 numPoints = leInt(rec + 40);
            const qint64 partsOff = 44;
            const qint64 pointsOff = partsOff + qint64(numParts) * 4;
            if (numParts <= 0 || numPoints <= 0)
                break;
            if (pointsOff + qint64(numPoints) * 16 > contentBytes)
                break;

            feat.parts.reserve(numParts);
            for (qint32 p = 0; p < numParts; ++p) {
                const qint32 start = leInt(rec + partsOff + p * 4);
                const qint32 end = (p + 1 < numParts) ? leInt(rec + partsOff + (p + 1) * 4) : numPoints;
                if (start < 0 || end > numPoints || end <= start)
                    continue;
                QVector<QPointF> pts;
                pts.reserve(end - start);
                for (qint32 i = start; i < end; ++i)
                    pts.append(QPointF(leDouble(rec + pointsOff + qint64(i) * 16),
                                       leDouble(rec + pointsOff + qint64(i) * 16 + 8)));
                feat.parts.append(pts);
            }
            break;
        }
        default:
            break; // kiểu Null hoặc biến thể Z/M chưa dùng tới
        }

        out->append(feat);
        off += contentBytes;
    }

    return true;
}

} // namespace ShpReader
