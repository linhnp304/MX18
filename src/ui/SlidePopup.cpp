#include "ui/SlidePopup.h"

#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QToolButton>
#include <QVBoxLayout>

SlidePopup::SlidePopup(const QString &title, Side side, QWidget *parent)
    : QFrame(parent)
    , m_side(side)
{
    setObjectName(QStringLiteral("SlidePopup"));
    setStyleSheet(QStringLiteral(
        "#SlidePopup { background: #1b1f24; border: 1px solid #3fa9f5; border-radius: 4px; }"));
    setFrameShape(QFrame::NoFrame);
    hide();

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 1, 1, 1);
    lay->setSpacing(0);

    auto *bar = new QWidget(this);
    bar->setStyleSheet(QStringLiteral("background:#262d35;"));
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(8, 4, 4, 4);
    auto *lbl = new QLabel(title, bar);
    lbl->setStyleSheet(QStringLiteral("color:#7fc4ff;font-weight:bold;background:transparent;"));
    auto *closeBtn = new QToolButton(bar);
    closeBtn->setIcon(IconFactory::icon(IconFactory::Glyph::Clear, Theme::kTextDim, 12));
    closeBtn->setAutoRaise(true);
    closeBtn->setToolTip(QStringLiteral("Đóng"));
    barLay->addWidget(lbl);
    barLay->addStretch(1);
    barLay->addWidget(closeBtn);
    connect(closeBtn, &QToolButton::clicked, this, &SlidePopup::closePopup);

    m_body = new QWidget(this);
    lay->addWidget(bar);
    lay->addWidget(m_body, 1);

    m_anim = new QPropertyAnimation(this, "pos", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
}

void SlidePopup::openAt(const QPoint &targetTopLeft)
{
    m_target = targetTopLeft;
    adjustSize();

    // Điểm xuất phát nằm ngoài mép cửa sổ ở phía cửa sổ popup neo vào.
    const int startX = (m_side == FromLeft) ? -width()
                                            : (parentWidget() ? parentWidget()->width() : width());
    move(startX, m_target.y());
    show();
    raise();
    m_open = true;

    m_anim->stop();
    m_anim->setStartValue(QPoint(startX, m_target.y()));
    m_anim->setEndValue(m_target);
    m_anim->disconnect(this);
    m_anim->start();
}

void SlidePopup::closePopup()
{
    if (!m_open)
        return;
    m_open = false;

    const int endX = (m_side == FromLeft) ? -width()
                                          : (parentWidget() ? parentWidget()->width() : width());
    m_anim->stop();
    m_anim->disconnect(this);
    m_anim->setStartValue(pos());
    m_anim->setEndValue(QPoint(endX, y()));
    connect(m_anim, &QPropertyAnimation::finished, this, [this] {
        hide();
        emit closed();
    });
    m_anim->start();
}
