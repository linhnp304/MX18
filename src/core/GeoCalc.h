#pragma once

#include <QPointF>

// Toạ độ địa lý WGS84, đơn vị độ.
struct GeoPoint {
    double lat = 0.0;
    double lon = 0.0;
};

// Phép chiếu phương vị cách đều (azimuthal equidistant) lấy tâm đài làm gốc.
//
// Chọn phép chiếu này vì màn hình trắc thủ lấy tâm đài làm chuẩn: cự ly đo trên
// mặt phẳng chiếu đúng bằng cự ly thực, phương vị đúng bằng phương vị thực, nên
// vòng cự ly là đường tròn chính xác và đường chia độ là đoạn thẳng — con số
// hiển thị trên thanh trạng thái và vị trí vẽ không bao giờ lệch nhau.
//
// Mặt phẳng dùng đơn vị km, x hướng đông, y hướng bắc.
class LocalProjection
{
public:
    LocalProjection() { setCenter(21.202111, 105.813417); }

    void setCenter(double lat, double lon);
    double centerLat() const { return m_lat0Deg; }
    double centerLon() const { return m_lon0Deg; }

    QPointF toPlane(double lat, double lon) const;
    QPointF toPlane(const GeoPoint &g) const { return toPlane(g.lat, g.lon); }
    GeoPoint toGeo(const QPointF &plane) const;

    // Phương vị 0..360 độ (0 = bắc, chiều kim đồng hồ) và cự ly km tính từ tâm đài.
    void bearingRange(double lat, double lon, double *bearingDeg, double *rangeKm) const;

    // Điểm cách tâm đài rangeKm theo phương vị bearingDeg, trả về toạ độ mặt phẳng.
    static QPointF planeFromPolar(double bearingDeg, double rangeKm);

private:
    double m_lat0Deg = 0.0;
    double m_lon0Deg = 0.0;
    double m_phi0 = 0.0;   // rad
    double m_lam0 = 0.0;   // rad
    double m_sinPhi0 = 0.0;
    double m_cosPhi0 = 1.0;
    double m_radiusKm = 6371.0;
};
