#include "ui/TrackInfoBox.h"

#include "proto/Packets.h"
#include "track/TrackStore.h"
#include "ui/IconFactory.h"
#include "ui/TargetLayer.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString typeText(quint32 type)
{
    switch (type) {
    case Track::TypeVqMh: return QStringLiteral("RD-MH");
    case Track::TypeMh:   return QStringLiteral("MH");
    default:              return QStringLiteral("RD");
    }
}

} // namespace

TrackInfoBox::TrackInfoBox(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("TrackInfoBox"));
    // Nền đặc: chữ của ô thông tin điểm dấu bên dưới lẫn vào thì khó đọc. Theme
    // có luật QWidget { background } nên nhãn con phải đặt nền trong suốt.
    setStyleSheet(QStringLiteral(
        "#TrackInfoBox { background: #1b1f24; border: 1px solid #3fa9f5;"
        " border-radius: 4px; }"
        "#TrackInfoBox QLabel { background: transparent; }"
        "#TrackInfoBar { background: #262d35; border-top-left-radius: 4px;"
        " border-top-right-radius: 4px; }"));
    setCursor(Qt::ArrowCursor);
    hide();

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 1, 1, 1);
    lay->setSpacing(0);

    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("TrackInfoBar"));
    bar->setCursor(Qt::SizeAllCursor);
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(8, 3, 3, 3);
    auto *title = new QLabel(QStringLiteral("Thông tin quỹ đạo"), bar);
    title->setStyleSheet(QStringLiteral("color:#7fc4ff;font-weight:bold;"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *closeBtn = new QToolButton(bar);
    closeBtn->setIcon(IconFactory::icon(IconFactory::Glyph::Clear, Theme::kTextDim, 12));
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::ArrowCursor);
    // Không dùng đúng chữ "Đóng": mx18shot chọn nút đóng của các popup theo
    // tooltip đó và thứ tự xuất hiện.
    closeBtn->setToolTip(QStringLiteral("Đóng thông tin quỹ đạo"));
    barLay->addWidget(title);
    barLay->addStretch(1);
    barLay->addWidget(closeBtn);
    connect(closeBtn, &QToolButton::clicked, this, &TrackInfoBox::closeBox);

    m_body = new QLabel(this);
    m_body->setTextFormat(Qt::RichText);
    m_body->setContentsMargins(9, 6, 10, 8);
    // Nhãn rich text tự nhận sự kiện chuột (liên kết); để lọt lên cửa sổ cho kéo được.
    m_body->setAttribute(Qt::WA_TransparentForMouseEvents);
    lay->addWidget(bar);
    lay->addWidget(m_body);
}

void TrackInfoBox::setTrack(const TrackEntry &t)
{
    m_id = t.id();

    // Bảng hai cột: nhãn mờ bên trái, giá trị sáng bên phải; dòng lat-lng nằm
    // thẳng dưới phương vị - cự ly như ví dụ của step-06.
    QString html = QStringLiteral("<table cellspacing='0' cellpadding='1'>");
    const auto row = [&html](const QString &label, const QString &value) {
        html += QStringLiteral("<tr><td style='color:#8a95a1'>%1</td>"
                               "<td style='color:#e6edf4; padding-left:6px'>%2</td></tr>")
                    .arg(label, value);
    };
    const quint32 v = t.f[Track::Velocity];
    row(QStringLiteral("Tốp:"), QString::number(t.f[Track::TrackTop]));
    row(QStringLiteral("Loại:"), typeText(t.type()));
    row(QStringLiteral("Vị trí:"), QStringLiteral("%1° - %2km")
                                       .arg(t.azimuthDeg(), 0, 'f', 3)
                                       .arg(t.rangeM() / 1000.0, 0, 'f', 3));
    row(QString(), QStringLiteral("%1 - %2")
                       .arg(Track::latLng(t.f[Track::Lat]), 0, 'f', 6)
                       .arg(Track::latLng(t.f[Track::Lng]), 0, 'f', 6));
    row(QStringLiteral("Vận tốc:"), QStringLiteral("%1m/s (%2km/h)").arg(v).arg(qRound(v * 3.6)));
    row(QStringLiteral("Hướng:"), QStringLiteral("%1°").arg(t.f[Track::Heading] / 100.0, 0, 'f', 3));

    const quint32 mode = t.f[Track::IffReturnedMode];
    if (mode > 0) {
        html += QStringLiteral("<tr><td colspan='2' style='color:#7fc4ff; padding-top:5px'>"
                               "Thông tin nhận dạng</td></tr>");
        bool bold = false;
        const QString m = IffText::mode(mode, &bold);
        if (!m.isEmpty()) {
            html += QStringLiteral("<tr><td colspan='2' align='center' style='color:#e6edf4'>%1</td></tr>")
                        .arg(bold ? QStringLiteral("<b>%1</b>").arg(m) : m);
        }
        for (const QString &d : IffText::held(t.f[Track::IffCommander], t.f[Track::IffFlightid],
                                              t.f[Track::IffAltitude], t.f[Track::IffFuellevel])) {
            html += QStringLiteral("<tr><td colspan='2' style='color:#e6edf4; padding-left:14px'>%1</td></tr>")
                        .arg(d);
        }
    }
    html += QStringLiteral("</table>");

    m_body->setText(html);
    adjustSize();
    keepInside();
}

void TrackInfoBox::closeBox()
{
    if (!isVisible())
        return;
    hide();
    m_moved = false;
    m_id = 0;
    emit closed();
}

void TrackInfoBox::keepInside()
{
    const QWidget *p = parentWidget();
    if (!p)
        return;
    const int x = qBound(0, this->x(), qMax(0, p->width() - width()));
    const int y = qBound(0, this->y(), qMax(0, p->height() - height()));
    if (x != this->x() || y != this->y())
        move(x, y);
}

// ------------------------------------------------------------- kéo thả

void TrackInfoBox::mousePressEvent(QMouseEvent *event)
{
    // Nhận hết sự kiện chuột: bấm trong cửa sổ không được lọt xuống bản đồ
    // (sẽ thành kéo bản đồ hoặc chọn quỹ đạo nằm bên dưới).
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->position().toPoint();
    }
    event->accept();
}

void TrackInfoBox::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(mapToParent(event->position().toPoint()) - m_dragOffset);
        keepInside();
        m_moved = true;
    }
    event->accept();
}

void TrackInfoBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
    event->accept();
}
