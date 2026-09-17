#include "map/Projection.h"

#include <QFile>
#include <QRegularExpression>
#include <QtMath>

#include <cmath>

namespace {

constexpr double kD2R = M_PI / 180.0;
constexpr double kR2D = 180.0 / M_PI;

// Lấy PARAMETER["ten",gia_tri] trong chuỗi WKT.
double wktParam(const QString &wkt, const QString &name, double def)
{
    const QRegularExpression re(QStringLiteral("PARAMETER\\s*\\[\\s*\"%1\"\\s*,\\s*([-0-9.eE+]+)").arg(name),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(wkt);
    return m.hasMatch() ? m.captured(1).toDouble() : def;
}

// t trong công thức Snyder (hàm phụ của phép chiếu bảo giác).
double snyderT(double phi, double e)
{
    const double es = e * std::sin(phi);
    return std::tan(M_PI / 4.0 - phi / 2.0) / std::pow((1.0 - es) / (1.0 + es), e / 2.0);
}

double snyderM(double phi, double e2)
{
    const double s = std::sin(phi);
    return std::cos(phi) / std::sqrt(1.0 - e2 * s * s);
}

// Cung kinh tuyến từ xích đạo tới phi.
double meridionalArc(double phi, double a, double e2)
{
    const double e4 = e2 * e2;
    const double e6 = e4 * e2;
    return a * ((1.0 - e2 / 4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0) * phi
                - (3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * std::sin(2.0 * phi)
                + (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * std::sin(4.0 * phi)
                - (35.0 * e6 / 3072.0) * std::sin(6.0 * phi));
}

} // namespace

CoordTransform CoordTransform::fromPrjFile(const QString &prjPath)
{
    CoordTransform t;

    QFile f(prjPath);
    if (!f.open(QIODevice::ReadOnly))
        return t;
    const QString wkt = QString::fromLatin1(f.readAll());
    f.close();

    const QRegularExpression spheroidRe(
        QStringLiteral("SPHEROID\\s*\\[\\s*\"[^\"]*\"\\s*,\\s*([-0-9.eE+]+)\\s*,\\s*([-0-9.eE+]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch sm = spheroidRe.match(wkt);
    if (sm.hasMatch()) {
        t.m_a = sm.captured(1).toDouble();
        t.m_invF = sm.captured(2).toDouble();
    }

    if (!wkt.contains(QStringLiteral("PROJCS"), Qt::CaseInsensitive)) {
        t.m_kind = Geographic;
        return t;
    }

    if (wkt.contains(QStringLiteral("Lambert_Conformal_Conic"), Qt::CaseInsensitive))
        t.m_kind = LambertConformalConic;
    else if (wkt.contains(QStringLiteral("Transverse_Mercator"), Qt::CaseInsensitive))
        t.m_kind = TransverseMercator;
    else
        t.m_kind = Geographic;

    t.m_lat0 = wktParam(wkt, QStringLiteral("latitude_of_origin"), 0.0);
    t.m_lon0 = wktParam(wkt, QStringLiteral("central_meridian"), 0.0);
    t.m_sp1 = wktParam(wkt, QStringLiteral("standard_parallel_1"), t.m_lat0);
    t.m_sp2 = wktParam(wkt, QStringLiteral("standard_parallel_2"), t.m_sp1);
    t.m_k0 = wktParam(wkt, QStringLiteral("scale_factor"), 1.0);
    t.m_falseEasting = wktParam(wkt, QStringLiteral("false_easting"), 0.0);
    t.m_falseNorthing = wktParam(wkt, QStringLiteral("false_northing"), 0.0);

    // UNIT cuối cùng trong WKT là đơn vị tuyến tính của hệ chiếu (mét hay km),
    // các UNIT trước đó thuộc GEOGCS và tính bằng radian trên độ.
    const QRegularExpression unitRe(
        QStringLiteral("UNIT\\s*\\[\\s*\"[^\"]*\"\\s*,\\s*([-0-9.eE+]+)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = unitRe.globalMatch(wkt);
    while (it.hasNext())
        t.m_unitToMetre = it.next().captured(1).toDouble();
    if (t.m_unitToMetre <= 0.0)
        t.m_unitToMetre = 1.0;

    t.prepare();
    return t;
}

void CoordTransform::prepare()
{
    const double f = 1.0 / m_invF;
    m_e2 = f * (2.0 - f);
    m_e = std::sqrt(m_e2);
    m_ep2 = m_e2 / (1.0 - m_e2);

    if (m_kind == LambertConformalConic) {
        const double phi0 = m_lat0 * kD2R;
        const double phi1 = m_sp1 * kD2R;
        const double phi2 = m_sp2 * kD2R;

        const double m1 = snyderM(phi1, m_e2);
        const double m2 = snyderM(phi2, m_e2);
        const double t1 = snyderT(phi1, m_e);
        const double t2 = snyderT(phi2, m_e);
        const double t0 = snyderT(phi0, m_e);

        if (std::fabs(phi1 - phi2) < 1e-12)
            m_n = std::sin(phi1);
        else
            m_n = (std::log(m1) - std::log(m2)) / (std::log(t1) - std::log(t2));

        m_bigF = m1 / (m_n * std::pow(t1, m_n));
        m_rho0 = m_a * m_bigF * std::pow(t0, m_n);
    } else if (m_kind == TransverseMercator) {
        m_m0 = meridionalArc(m_lat0 * kD2R, m_a, m_e2);
    }
}

GeoPoint CoordTransform::toWgs84(double x, double y) const
{
    if (m_kind == Geographic)
        return GeoPoint{y, x}; // shapefile địa lý lưu (lon, lat)

    // Đưa về mét rồi bỏ false easting/northing (false cũng theo đơn vị của lớp).
    const double xm = (x - m_falseEasting) * m_unitToMetre;
    const double ym = (y - m_falseNorthing) * m_unitToMetre;

    if (m_kind == LambertConformalConic) {
        const double dy = m_rho0 - ym;
        double rho = std::hypot(xm, dy);
        if (m_n < 0.0)
            rho = -rho;
        if (std::fabs(rho) < 1e-12)
            return GeoPoint{(m_n > 0.0 ? 90.0 : -90.0), m_lon0};

        const double tp = std::pow(rho / (m_a * m_bigF), 1.0 / m_n);
        const double theta = std::atan2(m_n > 0.0 ? xm : -xm, m_n > 0.0 ? dy : -dy);

        // Lặp Snyder (7-9): hội tụ sau 4-5 vòng ở vĩ độ Việt Nam.
        double phi = M_PI / 2.0 - 2.0 * std::atan(tp);
        for (int i = 0; i < 12; ++i) {
            const double es = m_e * std::sin(phi);
            const double next = M_PI / 2.0
                                - 2.0 * std::atan(tp * std::pow((1.0 - es) / (1.0 + es), m_e / 2.0));
            if (std::fabs(next - phi) < 1e-12) {
                phi = next;
                break;
            }
            phi = next;
        }

        GeoPoint g;
        g.lat = phi * kR2D;
        g.lon = m_lon0 + (theta / m_n) * kR2D;
        return g;
    }

    // Transverse Mercator nghịch chuyển (Snyder 8-1..8-9).
    const double bigM = m_m0 + ym / m_k0;
    const double e1 = (1.0 - std::sqrt(1.0 - m_e2)) / (1.0 + std::sqrt(1.0 - m_e2));
    const double mu = bigM / (m_a * (1.0 - m_e2 / 4.0 - 3.0 * m_e2 * m_e2 / 64.0
                                     - 5.0 * m_e2 * m_e2 * m_e2 / 256.0));
    const double e1_2 = e1 * e1, e1_3 = e1_2 * e1, e1_4 = e1_3 * e1;
    const double phi1 = mu
                        + (3.0 * e1 / 2.0 - 27.0 * e1_3 / 32.0) * std::sin(2.0 * mu)
                        + (21.0 * e1_2 / 16.0 - 55.0 * e1_4 / 32.0) * std::sin(4.0 * mu)
                        + (151.0 * e1_3 / 96.0) * std::sin(6.0 * mu)
                        + (1097.0 * e1_4 / 512.0) * std::sin(8.0 * mu);

    const double sinP = std::sin(phi1), cosP = std::cos(phi1), tanP = std::tan(phi1);
    const double c1 = m_ep2 * cosP * cosP;
    const double t1 = tanP * tanP;
    const double w = 1.0 - m_e2 * sinP * sinP;
    const double n1 = m_a / std::sqrt(w);
    const double r1 = m_a * (1.0 - m_e2) / (w * std::sqrt(w));
    const double d = xm / (n1 * m_k0);

    const double d2 = d * d, d3 = d2 * d, d4 = d3 * d, d5 = d4 * d, d6 = d5 * d;
    const double phi = phi1 - (n1 * tanP / r1)
                                 * (d2 / 2.0
                                    - (5.0 + 3.0 * t1 + 10.0 * c1 - 4.0 * c1 * c1 - 9.0 * m_ep2) * d4 / 24.0
                                    + (61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * t1 * t1
                                       - 252.0 * m_ep2 - 3.0 * c1 * c1) * d6 / 720.0);
    const double lam = (d - (1.0 + 2.0 * t1 + c1) * d3 / 6.0
                        + (5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * c1 * c1 + 8.0 * m_ep2 + 24.0 * t1 * t1) * d5 / 120.0)
                       / cosP;

    GeoPoint g;
    g.lat = phi * kR2D;
    g.lon = m_lon0 + lam * kR2D;
    return g;
}
