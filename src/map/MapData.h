#pragma once

#include "core/GeoCalc.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

// Toàn bộ nền bản đồ số đọc từ ./maps/mc, đã quy về WGS84 và chuẩn bị sẵn hình
// học trên mặt phẳng tâm đài ở 4 mức chi tiết.

enum class LayerId {
    LandIslands,     // đất liền + đảo (nền)
    Provinces,       // ranh giới tỉnh Việt Nam
    Rivers,
    NationalArea,    // ranh giới quốc gia trên biển
    CoastLines,
    BorderLines,     // đường biên giới
    HoangSa,
    TruongSa,
    AirRoutes,       // đường bay dân dụng
    Count
};

// 4 mức chi tiết; mức 3 là hình học đầy đủ.
constexpr int kLodCount = 4;

struct MapFeature {
    QVector<QPolygonF> source;              // WGS84, x = kinh độ, y = vĩ độ
    QRectF bbox;                            // trên mặt phẳng tâm đài (km)
    QPainterPath lodPath[kLodCount];        // dùng cho lớp vùng
    QVector<QPolygonF> lodLines[kLodCount]; // dùng cho lớp đường
};

struct MapLayer {
    LayerId id = LayerId::Count;
    QString name;
    bool filled = false;
    QVector<MapFeature> features;
};

struct PlaceLabel {
    GeoPoint pos;
    QString name;
    QPointF plane;
};

struct AirportInfo {
    GeoPoint pos;
    QString name;
    int level = 0;        // 0: ngoài lãnh thổ, 1..3: sân bay cấp 1..3
    double heading = 0.0; // hướng cất hạ cánh, độ
    QPointF plane;
};

struct RouteLabel {
    GeoPoint pos;
    QString name;
    QPointF plane;
};

class MapData
{
public:
    // Đọc và quy đổi toàn bộ thư mục dữ liệu. Trả về false nếu không có lớp nào.
    bool load(const QString &mcDir);

    // Dựng lại hình học mặt phẳng + các mức chi tiết theo tâm đài mới.
    void project(const LocalProjection &proj);

    const QVector<MapLayer> &layers() const { return m_layers; }
    const QVector<PlaceLabel> &places() const { return m_places; }
    const QVector<AirportInfo> &airports() const { return m_airports; }
    const QVector<RouteLabel> &routeLabels() const { return m_routeLabels; }

    // Sai số cho phép (km) của từng mức chi tiết.
    static double lodTolerance(int lod);
    // Mức chi tiết nhỏ nhất còn đủ mịn ở tỉ lệ đang vẽ (pixel trên km).
    static int lodForScale(double pixelsPerKm);

    QString lastError() const { return m_error; }

private:
    void loadShapeLayer(const QString &dir, const QString &base, LayerId id,
                        const QString &name, bool filled);
    void loadAirRoutes(const QString &dir);
    void loadPlaces(const QString &dir);
    void loadAirports(const QString &dir);

    QVector<MapLayer> m_layers;
    QVector<PlaceLabel> m_places;
    QVector<AirportInfo> m_airports;
    QVector<RouteLabel> m_routeLabels;
    QString m_error;
};
