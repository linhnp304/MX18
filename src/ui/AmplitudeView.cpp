#include "ui/AmplitudeView.h"

#include "core/Settings.h"
#include "proto/Packets.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

namespace {

// Mỗi điểm biên độ phủ 600 m; điểm i nằm giữa i*0,6 km và (i+1)*0,6 km.
constexpr double kKmPerSample = kMaxRangeKm / Video::kSamples;

const QColor kTrace(0x3c, 0xff, 0x6a);   // xanh lá sáng, tương phản mạnh trên nền đen
const QColor kGrid(0x1e, 0x3a, 0x1e);
const QColor kLabel(0x6f, 0x8a, 0x74);

} // namespace

AmplitudeView::AmplitudeView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(70);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
}

void AmplitudeView::setTrace(const QByteArray &video)
{
    m_video = video;
    update();
}

void AmplitudeView::clearTrace()
{
    m_video.clear();
    update();
}

QRectF AmplitudeView::plotArea() const
{
    // Chừa 12 px dưới đáy cho nhãn cự ly.
    return QRectF(rect()).adjusted(2, 2, -2, -13);
}

void AmplitudeView::mouseMoveEvent(QMouseEvent *event)
{
    m_cursor = event->pos();
    m_hasCursor = plotArea().contains(m_cursor);
    update();
}

void AmplitudeView::leaveEvent(QEvent *event)
{
    m_hasCursor = false;
    update();
    QWidget::leaveEvent(event);
}

void AmplitudeView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    const QRectF area = plotArea();
    if (area.width() < 8 || area.height() < 8)
        return;

    // Lưới: mỗi ô ngang 36 km, mỗi ô dọc 64 mức biên độ.
    p.setPen(QPen(kGrid, 1));
    for (int i = 1; i < 10; ++i) {
        const double x = area.left() + area.width() * i / 10.0;
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
        const double y = area.top() + area.height() * i / 4.0;
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    QFont small = font();
    small.setPointSizeF(qMax(6.5, small.pointSizeF() - 2.0));
    p.setFont(small);
    p.setPen(kLabel);
    for (int i = 0; i <= 10; i += 2) {
        const double x = area.left() + area.width() * i / 10.0;
        const QString text = QStringLiteral("%1").arg(int(kMaxRangeKm * i / 10));
        p.drawText(QRectF(x - 22, area.bottom() + 1, 44, 12),
                   Qt::AlignHCenter | Qt::AlignTop, text);
    }

    if (m_video.size() < Video::kSamples) {
        p.setPen(QPen(kTrace, 1.2));
        p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));
        p.setPen(QColor(0x4a, 0x5a, 0x4a));
        p.drawText(area, Qt::AlignCenter, QStringLiteral("Cửa sổ biên độ"));
        return;
    }

    // Đường biên độ vẽ bằng drawPolyline: nhanh hơn QPainterPath cho 600 điểm
    // lặp lại ở tần suất cao.
    QPolygonF poly(Video::kSamples);
    const double dx = area.width() / (Video::kSamples - 1);
    const auto *raw = reinterpret_cast<const quint8 *>(m_video.constData());
    for (int i = 0; i < Video::kSamples; ++i) {
        poly[i] = QPointF(area.left() + dx * i,
                          area.bottom() - area.height() * raw[i] / 255.0);
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(kTrace, 1.0));
    p.drawPolyline(poly);
    p.setRenderHint(QPainter::Antialiasing, false);

    if (!m_hasCursor)
        return;

    // Giá trị tại con trỏ: "128 - 234km".
    const int index = qBound(0, int((m_cursor.x() - area.left()) / dx + 0.5), Video::kSamples - 1);
    const double x = area.left() + dx * index;
    p.setPen(QPen(QColor(0xff, 0xd2, 0x4d), 1, Qt::DashLine));
    p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));

    const QString text = QStringLiteral("%1 - %2km")
                             .arg(raw[index])
                             .arg(int((index + 1) * kKmPerSample));
    const QRectF box(qMin(x + 6, area.right() - 76), area.top() + 2, 74, 14);
    p.fillRect(box, QColor(0, 0, 0, 190));
    p.setPen(QColor(0xff, 0xd2, 0x4d));
    p.drawText(box, Qt::AlignCenter, text);
}
