#include "core/GeoCalc.h"

#include <QtMath>

#include <cmath>

namespace {
constexpr double kDeg2Rad = M_PI / 180.0;
constexpr double kRad2Deg = 180.0 / M_PI;
constexpr double kWgs84aKm = 6378.137;
constexpr double kWgs84f = 1.0 / 298.257223563;
} // namespace

void LocalProjection::setCenter(double lat, double lon)
{
    m_lat0Deg = lat;
    m_lon0Deg = lon;
    m_phi0 = lat * kDeg2Rad;
    m_lam0 = lon * kDeg2Rad;
    m_sinPhi0 = std::sin(m_phi0);
    m_cosPhi0 = std::cos(m_phi0);

    // Bán kính Gauss (trung bình nhân của bán kính kinh tuyến và pháp tuyến) tại
    // vĩ độ tâm đài: coi Trái Đất là mặt cầu bán kính này thì sai số cự ly trong
    // bán kính 400 km chỉ cỡ vài chục mét, đủ cho yêu cầu hiển thị 0,001 km.
    const double e2 = kWgs84f * (2.0 - kWgs84f);
    const double w = 1.0 - e2 * m_sinPhi0 * m_sinPhi0;
    m_radiusKm = kWgs84aKm * std::sqrt(1.0 - e2) / w;
}

QPointF LocalProjection::toPlane(double lat, double lon) const
{
    const double phi = lat * kDeg2Rad;
    const double dlam = (lon - m_lon0Deg) * kDeg2Rad;
    const double sinPhi = std::sin(phi);
    const double cosPhi = std::cos(phi);
    const double cosDlam = std::cos(dlam);

    double cosc = m_sinPhi0 * sinPhi + m_cosPhi0 * cosPhi * cosDlam;
    cosc = qBound(-1.0, cosc, 1.0);
    const double c = std::acos(cosc);
    const double sinc = std::sin(c);
    // k -> 1 khi c -> 0; khai triển tránh chia cho 0 ngay tại tâm đài.
    const double k = (sinc < 1e-12) ? 1.0 : (c / sinc);

    const double x = m_radiusKm * k * cosPhi * std::sin(dlam);
    const double y = m_radiusKm * k * (m_cosPhi0 * sinPhi - m_sinPhi0 * cosPhi * cosDlam);
    return QPointF(x, y);
}

GeoPoint LocalProjection::toGeo(const QPointF &plane) const
{
    const double rho = std::hypot(plane.x(), plane.y());
    if (rho < 1e-9)
        return GeoPoint{m_lat0Deg, m_lon0Deg};

    const double c = rho / m_radiusKm;
    const double sinc = std::sin(c);
    const double cosc = std::cos(c);

    double sinLat = cosc * m_sinPhi0 + plane.y() * sinc * m_cosPhi0 / rho;
    sinLat = qBound(-1.0, sinLat, 1.0);
    const double lat = std::asin(sinLat);
    const double lon = m_lam0 + std::atan2(plane.x() * sinc,
                                           rho * m_cosPhi0 * cosc - plane.y() * m_sinPhi0 * sinc);

    GeoPoint g;
    g.lat = lat * kRad2Deg;
    g.lon = lon * kRad2Deg;
    // Đưa kinh độ về (-180, 180] để hiển thị.
    while (g.lon > 180.0) g.lon -= 360.0;
    while (g.lon <= -180.0) g.lon += 360.0;
    return g;
}

void LocalProjection::bearingRange(double lat, double lon, double *bearingDeg, double *rangeKm) const
{
    const QPointF p = toPlane(lat, lon);
    if (rangeKm)
        *rangeKm = std::hypot(p.x(), p.y());
    if (bearingDeg) {
        double b = std::atan2(p.x(), p.y()) * kRad2Deg;
        if (b < 0.0)
            b += 360.0;
        *bearingDeg = b;
    }
}

QPointF LocalProjection::planeFromPolar(double bearingDeg, double rangeKm)
{
    const double a = bearingDeg * kDeg2Rad;
    return QPointF(rangeKm * std::sin(a), rangeKm * std::cos(a));
}
