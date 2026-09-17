#pragma once

#include "core/GeoCalc.h"

#include <QString>

// Quy đổi toạ độ của một lớp shapefile về WGS84.
//
// Các lớp trong ./maps/mc không cùng hệ toạ độ (Lambert Conformal Conic,
// Transverse Mercator, hoặc địa lý), nên phải đọc .prj rồi nghịch chuyển trước
// khi vẽ chung. Máy build không có PROJ nên công thức tự cài đặt theo Snyder.
class CoordTransform
{
public:
    enum Kind { Geographic, LambertConformalConic, TransverseMercator };

    // Không đọc được .prj thì coi như toạ độ địa lý (lon, lat) — đúng với đa số
    // lớp còn lại và không làm hỏng dữ liệu đã đúng sẵn.
    static CoordTransform fromPrjFile(const QString &prjPath);

    Kind kind() const { return m_kind; }
    bool isIdentity() const { return m_kind == Geographic; }

    // x, y theo đơn vị gốc của lớp (mét, km hoặc độ tuỳ .prj).
    GeoPoint toWgs84(double x, double y) const;

private:
    void prepare();

    Kind m_kind = Geographic;

    // Ellipsoid
    double m_a = 6378137.0;
    double m_invF = 298.257223563;
    double m_e2 = 0.0;
    double m_e = 0.0;

    // Tham số phép chiếu (độ / mét)
    double m_lat0 = 0.0;
    double m_lon0 = 0.0;
    double m_sp1 = 0.0;
    double m_sp2 = 0.0;
    double m_k0 = 1.0;
    double m_falseEasting = 0.0;
    double m_falseNorthing = 0.0;
    double m_unitToMetre = 1.0;

    // Hằng số dẫn xuất
    double m_n = 0.0;
    double m_bigF = 0.0;
    double m_rho0 = 0.0;
    double m_m0 = 0.0;
    double m_ep2 = 0.0;
};
