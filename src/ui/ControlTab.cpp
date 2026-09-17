#include "ui/ControlTab.h"

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

ControlTab::ControlTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *page = new QWidget(scroll);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    m_lockBtn = new QPushButton(page);
    m_lockBtn->setMinimumHeight(28);
    lay->addWidget(m_lockBtn);

    for (const QString &title : {QStringLiteral("Điều khiển ăng ten"),
                                 QStringLiteral("Điều khiển MH"),
                                 QStringLiteral("Mã hỏi đáp"),
                                 QStringLiteral("Điều khiển phát"),
                                 QStringLiteral("Hệ thống phát hiện"),
                                 QStringLiteral("Điều khiển dịch vụ")}) {
        QGroupBox *g = addGroup(title, page);
        lay->addWidget(g);
        m_groups.append(g);
    }
    lay->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll, 1);

    // Nút mức kỹ sư nằm ngoài vùng cuộn để luôn ở dưới cùng.
    m_engineerBtn = new QPushButton(QStringLiteral("Điều khiển mức kỹ sư"), this);
    m_engineerBtn->setMinimumHeight(28);
    auto *bottom = new QVBoxLayout;
    bottom->setContentsMargins(8, 6, 8, 8);
    bottom->addWidget(m_engineerBtn);
    outer->addLayout(bottom);

    connect(m_lockBtn, &QPushButton::clicked, this, [this] { setUnlocked(!m_unlocked); });
    connect(m_engineerBtn, &QPushButton::clicked, this, &ControlTab::engineerRequested);

    setUnlocked(false);
}

QGroupBox *ControlTab::addGroup(const QString &title, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    auto *lay = new QVBoxLayout(g);
    lay->setContentsMargins(8, 4, 8, 6);
    auto *placeholder = new QLabel(QStringLiteral("(các lệnh chi tiết bổ sung ở giai đoạn sau)"), g);
    placeholder->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    placeholder->setWordWrap(true);
    lay->addWidget(placeholder);
    return g;
}

void ControlTab::setUnlocked(bool unlocked)
{
    m_unlocked = unlocked;
    m_lockBtn->setText(unlocked ? QStringLiteral("Khóa điều khiển")
                                : QStringLiteral("Mở khóa điều khiển"));
    // Đổi màu chữ để nhìn lướt cũng biết đang khoá hay đang mở.
    m_lockBtn->setStyleSheet(unlocked ? QStringLiteral("color:#ff8a5c;font-weight:bold;")
                                      : QStringLiteral("color:#7ee08a;font-weight:bold;"));
    for (QGroupBox *g : std::as_const(m_groups))
        g->setEnabled(unlocked);
    emit lockChanged(unlocked);
}
