#include "ui/ControlPanel.h"

#include "ui/AmplitudeView.h"
#include "ui/ControlTab.h"
#include "ui/SettingsTab.h"

#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QWidget *placeholderTab(const QString &text, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *lay = new QVBoxLayout(w);
    auto *lbl = new QLabel(text, w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    lay->addWidget(lbl);
    return w;
}

} // namespace

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setChildrenCollapsible(false);

    m_tabs = new QTabWidget(m_splitter);
    m_tabs->setDocumentMode(true);

    // Tab "Danh sách": bảng quỹ đạo, cột dựng sẵn để giai đoạn sau đổ dữ liệu.
    auto *trackTable = new QTableWidget(0, 6, m_tabs);
    trackTable->setHorizontalHeaderLabels({QStringLiteral("Số hiệu"), QStringLiteral("Phương vị"),
                                           QStringLiteral("Cự ly"), QStringLiteral("Độ cao"),
                                           QStringLiteral("Tốc độ"), QStringLiteral("Hướng")});
    trackTable->verticalHeader()->setVisible(false);
    trackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    trackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trackTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_controlTab = new ControlTab(m_tabs);
    m_settingsTab = new SettingsTab(m_tabs);

    m_tabs->addTab(trackTable, QStringLiteral("Danh sách"));
    m_tabs->addTab(m_controlTab, QStringLiteral("Điều khiển"));
    m_tabs->addTab(placeholderTab(QStringLiteral("Ghi lưu và tái hiện dữ liệu\n(giai đoạn sau)"), m_tabs),
                   QStringLiteral("Ghi lưu"));
    m_tabs->addTab(m_settingsTab, QStringLiteral("Cài đặt"));
    m_tabs->setCurrentWidget(m_settingsTab);

    m_amplitude = new AmplitudeView(m_splitter);

    m_splitter->addWidget(m_tabs);
    m_splitter->addWidget(m_amplitude);
    m_splitter->setStretchFactor(0, 4);
    m_splitter->setStretchFactor(1, 1);
    // Panel 2.1 (các tab điều khiển) 80% chiều dọc, panel 2.2 (biên độ) 20%.
    m_splitter->setSizes({800, 200});

    lay->addWidget(m_splitter);
}
