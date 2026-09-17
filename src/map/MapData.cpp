#include "map/MapData.h"

#include "map/DbfReader.h"
#include "map/Projection.h"
#include "map/ShpReader.h"

#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>

#include <cmath>

namespace {

// Sai số của từng mức chi tiết, tính bằng km trên mặt phẳng tâm đài.
const double kLodTol[kLodCount] = {6.0, 1.5, 0.35, 0.0};

// Douglas-Peucker khử điểm thừa. Làm trên mặt phẳng km nên ngưỡng có nghĩa vật lý.
void simplifyInto(const QPolygonF &in, double tol, QPolygonF *out)
{
    const int n = in.size();
    if (n < 3 || tol <= 0.0) {
        *out = in;
        return;
    }

    QVector<bool> keep(n, false);
    keep[0] = true;
    keep[n - 1] = true;

    QVector<QPair<int, int>> stack;
    stack.append({0, n - 1});
    const double tol2 = tol * tol;

    while (!stack.isEmpty()) {
        const auto seg = stack.takeLast();
        const int a = seg.first, b = seg.second;
        if (b <= a + 1)
            continue;

        const QPointF pa = in.at(a), pb = in.at(b);
        const double dx = pb.x() - pa.x();
        const double dy = pb.y() - pa.y();
        const double len2 = dx * dx + dy * dy;

        int worst = -1;
        double worstD2 = 0.0;
        for (int i = a + 1; i < b; ++i) {
            const QPointF p = in.at(i);
            double d2;
            if (len2 < 1e-18) {
                const double ex = p.x() - pa.x(), ey = p.y() - pa.y();
                d2 = ex * ex + ey * ey;
            } else {
                const double cross = dx * (p.y() - pa.y()) - dy * (p.x() - pa.x());
                d2 = cross * cross / len2;
            }
            if (d2 > worstD2) {
                worstD2 = d2;
                worst = i;
            }
        }

        if (worst > 0 && worstD2 > tol2) {
            keep[worst] = true;
            stack.append({a, worst});
            stack.append({worst, b});
        }
    }

    out->clear();
    out->reserve(n / 2 + 2);
    for (int i = 0; i < n; ++i) {
        if (keep.at(i))
            out->append(in.at(i));
    }
}

QString readTextFile(const QString &path, QStringConverter::Encoding enc)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QStringDecoder dec(enc);
    const QString s = dec(f.readAll());
    f.close();
    return s;
}

} // namespace

double MapData::lodTolerance(int lod)
{
    return kLodTol[qBound(0, lod, kLodCount - 1)];
}

int MapData::lodForScale(double pixelsPerKm)
{
    // Chọn mức thô nhất mà sai số vẫn dưới ~1,2 pixel: mắt không phân biệt được
    // nhưng số điểm phải vẽ giảm hàng chục lần khi nhìn toàn cảnh.
    const double budgetKm = (pixelsPerKm > 1e-9) ? (1.2 / pixelsPerKm) : 1e9;
    for (int i = 0; i < kLodCount - 1; ++i) {
        if (kLodTol[i] <= budgetKm)
            return i;
    }
    return kLodCount - 1;
}

bool MapData::load(const QString &mcDir)
{
    m_layers.clear();
    m_places.clear();
    m_airports.clear();
    m_routeLabels.clear();
    m_error.clear();

    if (!QFileInfo::exists(mcDir)) {
        m_error = QStringLiteral("Không tìm thấy thư mục bản đồ: %1").arg(mcDir);
        return false;
    }

    // Thứ tự nạp cũng là thứ tự vẽ từ dưới lên. Nền panel để màu đất, lớp
    // "Ranh giới quốc gia" là vùng nước nên phải tô trước rồi mới đến đất liền.
    loadShapeLayer(mcDir, QStringLiteral("Ranhgoiquocgia"), LayerId::NationalArea,
                   QStringLiteral("Ranh giới quốc gia"), true);
    loadShapeLayer(mcDir, QStringLiteral("Land-Islands"), LayerId::LandIslands,
                   QStringLiteral("Đất liền và đảo"), true);
    loadShapeLayer(mcDir, QStringLiteral("VNM_adm1"), LayerId::Provinces,
                   QStringLiteral("Ranh giới tỉnh"), true);
    loadShapeLayer(mcDir, QStringLiteral("Rivers"), LayerId::Rivers,
                   QStringLiteral("Sông ngòi"), true);
    loadShapeLayer(mcDir, QStringLiteral("CoastLines"), LayerId::CoastLines,
                   QStringLiteral("Đường bờ biển"), false);
    loadShapeLayer(mcDir, QStringLiteral("Duongbiengioi"), LayerId::BorderLines,
                   QStringLiteral("Đường biên giới"), false);
    loadShapeLayer(mcDir, QStringLiteral("Hoang_Sa"), LayerId::HoangSa,
                   QStringLiteral("Hoàng Sa"), false);
    loadShapeLayer(mcDir, QStringLiteral("Truong_Sa"), LayerId::TruongSa,
                   QStringLiteral("Trường Sa"), false);
    loadAirRoutes(mcDir);

    loadPlaces(mcDir);
    loadAirports(mcDir);

    if (m_layers.isEmpty()) {
        m_error = QStringLiteral("Không đọc được lớp bản đồ nào trong %1").arg(mcDir);
        return false;
    }
    return true;
}

void MapData::loadShapeLayer(const QString &dir, const QString &base, LayerId id,
                             const QString &name, bool filled)
{
    const QString shp = dir + QLatin1Char('/') + base + QStringLiteral(".shp");
    QVector<ShpReader::Feature> feats;
    int shapeType = 0;
    if (!ShpReader::read(shp, &feats, &shapeType) || feats.isEmpty())
        return;

    const CoordTransform tr =
        CoordTransform::fromPrjFile(dir + QLatin1Char('/') + base + QStringLiteral(".prj"));

    MapLayer layer;
    layer.id = id;
    layer.name = name;
    layer.filled = filled;
    layer.features.reserve(feats.size());

    for (const ShpReader::Feature &f : std::as_const(feats)) {
        MapFeature mf;
        mf.source.reserve(f.parts.size());
        for (const QVector<QPointF> &part : f.parts) {
            if (part.size() < 2)
                continue;
            QPolygonF poly;
            poly.reserve(part.size());
            for (const QPointF &p : part) {
                const GeoPoint g = tr.toWgs84(p.x(), p.y());
                poly.append(QPointF(g.lon, g.lat));
            }
            mf.source.append(poly);
        }
        if (!mf.source.isEmpty())
            layer.features.append(mf);
    }

    if (!layer.features.isEmpty())
        m_layers.append(layer);
}

void MapData::loadAirRoutes(const QString &dir)
{
    const QString base = dir + QStringLiteral("/AirRoutes");
    QVector<ShpReader::Feature> feats;
    int shapeType = 0;
    if (!ShpReader::read(base + QStringLiteral(".shp"), &feats, &shapeType) || feats.isEmpty())
        return;

    const CoordTransform tr = CoordTransform::fromPrjFile(base + QStringLiteral(".prj"));
    DbfReader dbf;
    const bool haveNames = dbf.open(base + QStringLiteral(".dbf"));

    MapLayer layer;
    layer.id = LayerId::AirRoutes;
    layer.name = QStringLiteral("Đường bay dân dụng");
    layer.filled = false;
    layer.features.reserve(feats.size());

    for (int i = 0; i < feats.size(); ++i) {
        MapFeature mf;
        for (const QVector<QPointF> &part : feats.at(i).parts) {
            if (part.size() < 2)
                continue;
            QPolygonF poly;
            poly.reserve(part.size());
            for (const QPointF &p : part) {
                const GeoPoint g = tr.toWgs84(p.x(), p.y());
                poly.append(QPointF(g.lon, g.lat));
            }
            mf.source.append(poly);
        }
        if (mf.source.isEmpty())
            continue;

        if (haveNames) {
            const QString nm = dbf.value(i, QStringLiteral("name")).trimmed();
            if (!nm.isEmpty()) {
                // Nhãn đặt ở giữa đoạn đường bay để không đè lên đầu mút.
                const QPolygonF &poly = mf.source.first();
                const QPointF mid = poly.at(poly.size() / 2);
                m_routeLabels.append(RouteLabel{GeoPoint{mid.y(), mid.x()}, nm, QPointF()});
            }
        }
        layer.features.append(mf);
    }

    if (!layer.features.isEmpty())
        m_layers.append(layer);
}

void MapData::loadPlaces(const QString &dir)
{
    // Diadanh.txt là UTF-16LE, CRLF, cột cách nhau bằng tab: tên, lat, lng.
    const QString text = readTextFile(dir + QStringLiteral("/Diadanh.txt"),
                                      QStringConverter::Utf16LE);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        QString line = raw;
        line.remove(QLatin1Char('\r'));
        line.remove(QChar(0xFEFF)); // BOM còn sót ở dòng đầu
        const QStringList col = line.split(QLatin1Char('\t'));
        if (col.size() < 3)
            continue;
        bool okLat = false, okLon = false;
        const double lat = col.at(1).trimmed().toDouble(&okLat);
        const double lon = col.at(2).trimmed().toDouble(&okLon);
        if (!okLat || !okLon)
            continue;
        m_places.append(PlaceLabel{GeoPoint{lat, lon}, col.at(0).trimmed(), QPointF()});
    }
}

void MapData::loadAirports(const QString &dir)
{
    // Airport2.dat là UTF-8 có BOM, CRLF, tab. Cột 0/1/2 là tên/lat/lng, hai cột
    // cuối là hướng cất hạ cánh (độ) và cấp sân bay (0 = ngoài lãnh thổ).
    const QString text = readTextFile(dir + QStringLiteral("/Airport2.dat"),
                                      QStringConverter::Utf8);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        QString line = raw;
        line.remove(QLatin1Char('\r'));
        line.remove(QChar(0xFEFF));
        const QStringList col = line.split(QLatin1Char('\t'));
        if (col.size() < 15)
            continue;
        bool okLat = false, okLon = false;
        const double lat = col.at(1).trimmed().toDouble(&okLat);
        const double lon = col.at(2).trimmed().toDouble(&okLon);
        if (!okLat || !okLon)
            continue;

        AirportInfo a;
        a.pos = GeoPoint{lat, lon};
        a.name = col.at(0).trimmed();
        a.heading = col.at(col.size() - 2).trimmed().toDouble();
        a.level = qBound(0, col.at(col.size() - 1).trimmed().toInt(), 3);
        m_airports.append(a);
    }
}

void MapData::project(const LocalProjection &proj)
{
    for (MapLayer &layer : m_layers) {
        for (MapFeature &f : layer.features) {
            // Mức đầy đủ trước, các mức thô hơn rút gọn từ mức đầy đủ.
            QVector<QPolygonF> full;
            full.reserve(f.source.size());
            QRectF bbox;
            for (const QPolygonF &src : std::as_const(f.source)) {
                QPolygonF plane;
                plane.reserve(src.size());
                for (const QPointF &g : src)
                    plane.append(proj.toPlane(g.y(), g.x()));
                full.append(plane);
                bbox = bbox.isNull() ? plane.boundingRect() : bbox.united(plane.boundingRect());
            }
            f.bbox = bbox;

            for (int lod = 0; lod < kLodCount; ++lod) {
                QVector<QPolygonF> simplified;
                if (lod == kLodCount - 1) {
                    simplified = full;
                } else {
                    simplified.reserve(full.size());
                    for (const QPolygonF &poly : std::as_const(full)) {
                        QPolygonF s;
                        simplifyInto(poly, kLodTol[lod], &s);
                        if (s.size() >= 2)
                            simplified.append(s);
                    }
                }

                if (layer.filled) {
                    QPainterPath path;
                    path.setFillRule(Qt::OddEvenFill);
                    for (const QPolygonF &poly : std::as_const(simplified)) {
                        if (poly.size() < 3)
                            continue;
                        path.addPolygon(poly);
                        path.closeSubpath(); // để nét viền vùng không hở ở điểm nối
                    }
                    f.lodPath[lod] = path;
                    f.lodLines[lod].clear();
                } else {
                    f.lodLines[lod] = simplified;
                    f.lodPath[lod] = QPainterPath();
                }
            }
        }
    }

    for (PlaceLabel &p : m_places)
        p.plane = proj.toPlane(p.pos);
    for (AirportInfo &a : m_airports)
        a.plane = proj.toPlane(a.pos);
    for (RouteLabel &r : m_routeLabels)
        r.plane = proj.toPlane(r.pos);
}
