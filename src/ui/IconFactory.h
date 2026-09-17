#pragma once

#include <QColor>
#include <QIcon>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>

class QPainter;

// Mọi biểu tượng đều dựng bằng QPainterPath thay vì file ảnh: các nút trạng thái
// phải đổi màu theo trạng thái mà QIcon nạp từ ảnh thì không tự nhuộm màu được,
// và dự án không dùng module Qt Svg.
namespace IconFactory {

enum class Glyph {
    Notification,  // thông báo hệ thống (chuông)
    Network,       // trạng thái kết nối mạng LAN
    Lock,          // trạng thái MH (ổ khoá)
    RadarAntenna,  // trạng thái SCN (ăng ten ra đa)
    Service,       // trạng thái SVR (bánh răng dịch vụ)
    ArrowsRight,   // ẩn bảng điều khiển
    ArrowsLeft,    // hiện bảng điều khiển
    RadarCenter,   // toạ độ tâm đài
    Clear,         // xoá
};

void draw(QPainter *p, Glyph g, const QRectF &box, const QColor &color);
QIcon icon(Glyph g, const QColor &color, int size = 20);

// Ký hiệu sân bay trên bản đồ.
//  level 0        : sân bay ngoài lãnh thổ (ô vuông xanh + hình máy bay)
//  level 1, 2, 3  : sân bay cấp 1/2/3 trong nước (đĩa tròn + dải đường băng
//                   trong suốt, cấp 1 có hai vòng ngoài, cấp 2 có một vòng)
// headingDeg là hướng cất hạ cánh, dùng để xoay dải đường băng / mũi máy bay.
void drawAirport(QPainter *p, const QPointF &center, int level, double headingDeg,
                 double size, const QColor &color);

// Ký hiệu tâm đài trên bản đồ.
void drawRadarSite(QPainter *p, const QPointF &center, double size, const QColor &color);

} // namespace IconFactory
