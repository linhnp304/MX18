#include "ui/AmplitudeView.h"

#include <QPainter>
#include <QPainterPath>

AmplitudeView::AmplitudeView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(70);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void AmplitudeView::setTrace(const QVector<float> &samples)
{
    m_samples = samples;
    update();
}

void AmplitudeView::clearTrace()
{
    m_samples.clear();
    update();
}

void AmplitudeView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    const QRectF area = QRectF(rect()).adjusted(2, 2, -2, -2);

    // Lưới mờ để trắc thủ ước lượng cự ly và mức tín hiệu.
    p.setPen(QPen(QColor(0x1e, 0x3a, 0x1e), 1));
    for (int i = 1; i < 10; ++i) {
        const double x = area.left() + area.width() * i / 10.0;
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }
    for (int i = 1; i < 4; ++i) {
        const double y = area.top() + area.height() * i / 4.0;
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor trace(0x3c, 0xff, 0x6a); // xanh lá sáng, tương phản mạnh trên nền đen

    if (m_samples.isEmpty()) {
        p.setPen(QPen(trace, 1.2));
        p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));
        p.setPen(QColor(0x4a, 0x5a, 0x4a));
        p.drawText(area, Qt::AlignCenter, QStringLiteral("Cửa sổ biên độ"));
        return;
    }

    QPainterPath path;
    for (int i = 0; i < m_samples.size(); ++i) {
        const double x = area.left() + area.width() * i / double(m_samples.size() - 1);
        const double y = area.bottom() - area.height() * qBound(0.0f, m_samples.at(i), 1.0f);
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    p.setPen(QPen(trace, 1.2));
    p.drawPath(path);
}
