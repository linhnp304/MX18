#include "ui/StatusPanel.h"

#include "ui/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QToolButton>

#include <cmath>

namespace {

QColor colorOf(StatusPanel::StateColor c)
{
    switch (c) {
    case StatusPanel::Ok:    return Theme::kOk;
    case StatusPanel::Warn:  return Theme::kWarn;
    case StatusPanel::Error: return Theme::kError;
    case StatusPanel::Idle:
    default:                 return Theme::kIdle;
    }
}

} // namespace

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("StatusPanel"));
    setStyleSheet(QStringLiteral(
        "#StatusPanel { background: #1b1f24; border-top: 1px solid #353d46; }"
        "#StatusPanel QToolButton { background: transparent; border: 1px solid transparent;"
        " border-radius: 3px; padding: 2px; }"
        "#StatusPanel QToolButton:hover { background: #2a313a; }"
        "#StatusPanel QToolButton:checked { background: #2f6ea5; border-color: #3fa9f5; }"));
    setFixedHeight(30);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 2, 6, 2);
    lay->setSpacing(3);

    lay->addWidget(makeButton(Notify, IconFactory::Glyph::Notification,
                              QStringLiteral("Thông báo hệ thống")));
    lay->addWidget(makeButton(Network, IconFactory::Glyph::Network,
                              QStringLiteral("Trạng thái kết nối mạng")));
    lay->addWidget(makeButton(MhStatus, IconFactory::Glyph::Lock,
                              QStringLiteral("Trạng thái MH")));
    lay->addWidget(makeButton(ScnStatus, IconFactory::Glyph::RadarAntenna,
                              QStringLiteral("Trạng thái SCN")));
    lay->addWidget(makeButton(SvrStatus, IconFactory::Glyph::Service,
                              QStringLiteral("Trạng thái SVR")));

    lay->addStretch(1);

    m_timeLabel = new QLabel(this);
    m_cursorLabel = new QLabel(this);
    m_angleLabel = new QLabel(this);
    for (QLabel *l : {m_timeLabel, m_cursorLabel, m_angleLabel}) {
        l->setStyleSheet(QStringLiteral("background:transparent;color:#c3cdd7;"));
        l->setTextFormat(Qt::RichText);
    }
    m_timeLabel->setMinimumWidth(150);
    m_cursorLabel->setMinimumWidth(330);
    m_angleLabel->setMinimumWidth(240);

    lay->addWidget(m_timeLabel);
    lay->addSpacing(14);
    lay->addWidget(m_cursorLabel);
    lay->addSpacing(14);
    lay->addWidget(m_angleLabel);

    lay->addStretch(1);

    lay->addWidget(makeButton(RadarCenter, IconFactory::Glyph::RadarCenter,
                              QStringLiteral("Tọa độ tâm đài")));

    m_panelBtn = new QToolButton(this);
    m_panelBtn->setToolTip(QStringLiteral("Ẩn/Hiện bảng điều khiển"));
    m_panelBtn->setIconSize(QSize(18, 18));
    m_panelBtn->setIcon(IconFactory::icon(IconFactory::Glyph::ArrowsRight, Theme::kIdle, 18));
    connect(m_panelBtn, &QToolButton::clicked, this, &StatusPanel::panelToggleRequested);
    lay->addWidget(m_panelBtn);

    setCursorInfo(false, 0, 0, 0, 0);
    setSweepAngles(false, 0, 0);

    auto *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, &StatusPanel::updateTime);
    clock->start(250);
    updateTime();
}

QToolButton *StatusPanel::makeButton(PopupId id, IconFactory::Glyph glyph, const QString &tooltip)
{
    auto *b = new QToolButton(this);
    b->setToolTip(tooltip);
    b->setCheckable(true);
    b->setIconSize(QSize(18, 18));
    b->setIcon(IconFactory::icon(glyph, Theme::kIdle, 18));
    connect(b, &QToolButton::clicked, this, [this, id] { emit popupToggled(int(id)); });
    m_buttons[id] = b;
    m_glyphs[id] = glyph;
    return b;
}

void StatusPanel::setPopupState(PopupId id, StateColor color)
{
    if (id < 0 || id >= PopupCount || !m_buttons[id])
        return;
    m_buttons[id]->setIcon(IconFactory::icon(m_glyphs[id], colorOf(color), 18));
}

void StatusPanel::setPopupChecked(PopupId id, bool checked)
{
    if (id >= 0 && id < PopupCount && m_buttons[id])
        m_buttons[id]->setChecked(checked);
}

void StatusPanel::setPanelHidden(bool hidden)
{
    m_panelHidden = hidden;
    // Panel đang hiện -> mũi tên chỉ sang phải (bấm để đẩy đi), và ngược lại.
    m_panelBtn->setIcon(IconFactory::icon(
        hidden ? IconFactory::Glyph::ArrowsLeft : IconFactory::Glyph::ArrowsRight,
        Theme::kIdle, 18));
}

QPoint StatusPanel::anchorFor(PopupId id, QWidget *reference) const
{
    if (id < 0 || id >= PopupCount || !m_buttons[id] || !reference)
        return QPoint();
    const QWidget *b = m_buttons[id];
    return reference->mapFromGlobal(b->mapToGlobal(QPoint(0, 0)));
}

void StatusPanel::updateTime()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")));
}

void StatusPanel::setCursorInfo(bool valid, double lat, double lon, double bearing, double range)
{
    if (!valid) {
        m_cursorLabel->setText(QStringLiteral("Con trỏ: —"));
        return;
    }
    m_cursorLabel->setText(QStringLiteral("Con trỏ: %1-%2 &nbsp;&nbsp; %3-%4")
                               .arg(QString::number(lat, 'f', 6),
                                    QString::number(lon, 'f', 6),
                                    QString::number(bearing, 'f', 3),
                                    QString::number(range, 'f', 3)));
}

void StatusPanel::setSweepAngles(bool valid, double radarAz, double mhAz)
{
    const double rd = valid ? radarAz : 0.0;
    const double mh = valid ? mhAz : 0.0;
    const double delta = std::fabs(rd - mh);
    // Lệch quá nửa độ là dấu hiệu MH bám không kịp ăng ten -> tô đỏ cho nổi.
    const QString deltaHtml = (delta > 0.5)
        ? QStringLiteral("<span style='color:#ff4d4d;'>%1</span>").arg(QString::number(delta, 'f', 3))
        : QString::number(delta, 'f', 3);
    m_angleLabel->setText(QStringLiteral("Góc: %1-%2 (%3)")
                              .arg(QString::number(rd, 'f', 3),
                                   QString::number(mh, 'f', 3), deltaHtml));
}
