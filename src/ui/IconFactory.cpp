#include "ui/IconFactory.h"

#include <QPainter>
#include <QPixmap>
#include <QTransform>
#include <QtMath>

#include <cmath>

namespace {

// Mọi glyph vẽ trong hộp chuẩn 100x100 rồi co về kích thước thật, nhờ vậy nét
// và tỉ lệ giữ nguyên ở mọi cỡ nút.
constexpr double kUnit = 100.0;

QPainterPath bellPath()
{
    QPainterPath p;
    p.moveTo(28, 68);
    p.cubicTo(28, 40, 34, 30, 50, 26);
    p.cubicTo(66, 30, 72, 40, 72, 68);
    p.lineTo(78, 76);
    p.lineTo(22, 76);
    p.closeSubpath();
    p.addEllipse(QPointF(50, 22), 6, 6);
    QPainterPath clap;
    clap.moveTo(42, 80);
    clap.cubicTo(42, 90, 58, 90, 58, 80);
    clap.closeSubpath();
    return p.united(clap);
}

QPainterPath networkPath()
{
    QPainterPath p;
    p.addRoundedRect(QRectF(37, 12, 26, 18), 3, 3);
    p.addRoundedRect(QRectF(12, 70, 26, 18), 3, 3);
    p.addRoundedRect(QRectF(62, 70, 26, 18), 3, 3);
    // Đường nối hình chữ T giữa nút trên và hai nút dưới.
    p.addRect(QRectF(47, 30, 6, 22));
    p.addRect(QRectF(25, 49, 50, 6));
    p.addRect(QRectF(22, 52, 6, 18));
    p.addRect(QRectF(72, 52, 6, 18));
    return p;
}

QPainterPath lockPath()
{
    QPainterPath body;
    body.addRoundedRect(QRectF(24, 46, 52, 42), 5, 5);

    QPainterPath shackle;
    shackle.moveTo(34, 48);
    shackle.lineTo(34, 34);
    shackle.arcTo(QRectF(34, 14, 32, 40), 180, -180);
    shackle.lineTo(66, 48);
    shackle.lineTo(58, 48);
    shackle.lineTo(58, 34);
    shackle.arcTo(QRectF(42, 22, 16, 24), 0, 180);
    shackle.lineTo(42, 48);
    shackle.closeSubpath();

    QPainterPath hole;
    hole.addEllipse(QPointF(50, 62), 6, 6);
    hole.addRect(QRectF(47, 62, 6, 14));

    return body.united(shackle).subtracted(hole);
}

QPainterPath radarPath()
{
    QPainterPath p;
    // Chân đế
    p.addRect(QRectF(44, 74, 12, 16));
    p.addRoundedRect(QRectF(30, 88, 40, 8), 3, 3);
    // Chảo ăng ten nghiêng
    QPainterPath dish;
    dish.moveTo(0, 0);
    dish.arcTo(QRectF(-34, -12, 68, 24), 0, 180);
    dish.closeSubpath();
    QTransform t;
    t.translate(50, 44);
    t.rotate(-30);
    p.addPath(t.map(dish));
    // Sóng quét
    QPainterPath wave;
    for (int i = 0; i < 2; ++i) {
        const double r = 14 + i * 11;
        wave.moveTo(64 + r, 26);
        wave.arcTo(QRectF(64 - r, 26 - r, 2 * r, 2 * r), 0, 60);
    }
    QPainterPathStroker st;
    st.setWidth(5);
    p.addPath(st.createStroke(wave));
    return p;
}

QPainterPath gearPath()
{
    QPainterPath p;
    const int teeth = 8;
    const double rOut = 44, rIn = 33;
    for (int i = 0; i < teeth; ++i) {
        const double a0 = i * 2 * M_PI / teeth;
        QPainterPath tooth;
        tooth.addRoundedRect(QRectF(-8, -rOut, 16, rOut - rIn + 10), 2, 2);
        QTransform t;
        t.translate(50, 50);
        t.rotate(a0 * 180.0 / M_PI);
        p.addPath(t.map(tooth));
    }
    QPainterPath disc;
    disc.addEllipse(QPointF(50, 50), rIn, rIn);
    QPainterPath hole;
    hole.addEllipse(QPointF(50, 50), 14, 14);
    return p.united(disc).subtracted(hole);
}

QPainterPath chevronsPath(bool toRight)
{
    QPainterPath arrows;
    for (int i = 0; i < 2; ++i) {
        const double x = 24 + i * 28;
        QPainterPath a;
        a.moveTo(x, 22);
        a.lineTo(x + 22, 50);
        a.lineTo(x, 78);
        arrows.addPath(a);
    }
    QPainterPathStroker st;
    st.setWidth(11);
    st.setCapStyle(Qt::RoundCap);
    st.setJoinStyle(Qt::RoundJoin);
    QPainterPath solid = st.createStroke(arrows);
    if (!toRight) {
        QTransform t;
        t.translate(100, 0);
        t.scale(-1, 1);
        solid = t.map(solid);
    }
    return solid;
}

QPainterPath radarCenterPath()
{
    QPainterPath rings;
    rings.addEllipse(QPointF(50, 50), 34, 34);
    rings.addEllipse(QPointF(50, 50), 16, 16);
    rings.moveTo(6, 50);
    rings.lineTo(94, 50);
    rings.moveTo(50, 6);
    rings.lineTo(50, 94);
    QPainterPathStroker st;
    st.setWidth(7);
    return st.createStroke(rings);
}

QPainterPath clearPath()
{
    QPainterPath x;
    x.moveTo(26, 26);
    x.lineTo(74, 74);
    x.moveTo(74, 26);
    x.lineTo(26, 74);
    QPainterPathStroker st;
    st.setWidth(12);
    st.setCapStyle(Qt::RoundCap);
    return st.createStroke(x);
}

QPainterPath glyphPath(IconFactory::Glyph g)
{
    switch (g) {
    case IconFactory::Glyph::Notification: return bellPath();
    case IconFactory::Glyph::Network:      return networkPath();
    case IconFactory::Glyph::Lock:         return lockPath();
    case IconFactory::Glyph::RadarAntenna: return radarPath();
    case IconFactory::Glyph::Service:      return gearPath();
    case IconFactory::Glyph::ArrowsRight:  return chevronsPath(true);
    case IconFactory::Glyph::ArrowsLeft:   return chevronsPath(false);
    case IconFactory::Glyph::RadarCenter:  return radarCenterPath();
    case IconFactory::Glyph::Clear:        return clearPath();
    }
    return QPainterPath();
}

} // namespace

namespace IconFactory {

void draw(QPainter *p, Glyph g, const QRectF &box, const QColor &color)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->translate(box.topLeft());
    p->scale(box.width() / kUnit, box.height() / kUnit);
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPath(glyphPath(g));
    p->restore();
}

QIcon icon(Glyph g, const QColor &color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    draw(&p, g, QRectF(0, 0, size, size), color);
    p.end();
    return QIcon(pm);
}

void drawAirport(QPainter *p, const QPointF &center, int level, double headingDeg,
                 double size, const QColor &color)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->translate(center);

    if (level <= 0) {
        // Sân bay ngoài lãnh thổ: ô vuông bo góc + bóng máy bay xoay theo hướng.
        const double h = size * 0.5;
        p->setPen(QPen(color.lighter(140), 1.0));
        p->setBrush(color);
        p->drawRoundedRect(QRectF(-h, -h, size, size), size * 0.12, size * 0.12);

        p->rotate(headingDeg);
        const double s = size / 48.0;
        QPainterPath plane;
        plane.moveTo(0, -17 * s);
        plane.cubicTo(3.2 * s, -14 * s, 3.6 * s, -8 * s, 3.6 * s, -3 * s);
        plane.lineTo(18 * s, 5 * s);
        plane.lineTo(18 * s, 9 * s);
        plane.lineTo(3.6 * s, 5.5 * s);
        plane.lineTo(3.6 * s, 12 * s);
        plane.lineTo(7 * s, 15.5 * s);
        plane.lineTo(7 * s, 17.5 * s);
        plane.lineTo(0, 15.5 * s);
        plane.lineTo(-7 * s, 17.5 * s);
        plane.lineTo(-7 * s, 15.5 * s);
        plane.lineTo(-3.6 * s, 12 * s);
        plane.lineTo(-3.6 * s, 5.5 * s);
        plane.lineTo(-18 * s, 9 * s);
        plane.lineTo(-18 * s, 5 * s);
        plane.lineTo(-3.6 * s, -3 * s);
        plane.cubicTo(-3.6 * s, -8 * s, -3.2 * s, -14 * s, 0, -17 * s);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0xf2, 0xf6, 0xff));
        p->drawPath(plane);
        p->restore();
        return;
    }

    // Sân bay trong nước: đĩa tròn khoét dải đường băng theo hướng cất hạ cánh.
    const double r = size * 0.32;
    QPainterPath disc;
    disc.addEllipse(QPointF(0, 0), r, r);

    QPainterPath runway;
    QTransform t;
    t.rotate(headingDeg);
    runway.addRect(QRectF(-r * 0.17, -r * 1.1, r * 0.34, r * 2.2));
    disc = disc.subtracted(t.map(runway));

    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawPath(disc);

    // Cấp 1 có hai vòng ngoài, cấp 2 một vòng, cấp 3 không vòng.
    const int rings = (level == 1) ? 2 : (level == 2 ? 1 : 0);
    p->setBrush(Qt::NoBrush);
    for (int i = 0; i < rings; ++i) {
        const double rr = r * (1.28 + i * 0.30);
        p->setPen(QPen(color, qMax(1.0, size * 0.055)));
        p->drawEllipse(QPointF(0, 0), rr, rr);
    }
    p->restore();
}

void drawRadarSite(QPainter *p, const QPointF &center, double size, const QColor &color)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->translate(center);
    p->setPen(QPen(color, qMax(1.0, size * 0.09)));
    p->setBrush(Qt::NoBrush);
    // Vòng tròn nhỏ + 8 tia: đủ nổi bật mà không che điểm dấu quanh tâm đài.
    p->drawEllipse(QPointF(0, 0), size * 0.22, size * 0.22);
    for (int i = 0; i < 8; ++i) {
        const double a = i * M_PI / 4.0;
        const double c = std::cos(a), s = std::sin(a);
        p->drawLine(QPointF(c * size * 0.32, s * size * 0.32),
                    QPointF(c * size * 0.55, s * size * 0.55));
    }
    p->restore();
}

} // namespace IconFactory
