#include "ui/ControlPanel.h"

#include "ui/AmplitudeView.h"
#include "ui/ControlTab.h"
#include "ui/RecordTab.h"
#include "ui/SettingsTab.h"
#include "ui/TrackListTab.h"

#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

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

    m_trackListTab = new TrackListTab(m_tabs);
    m_controlTab = new ControlTab(m_tabs);
    m_recordTab = new RecordTab(m_tabs);
    m_settingsTab = new SettingsTab(m_tabs);

    m_tabs->addTab(m_trackListTab, QStringLiteral("Danh sách"));
    m_tabs->addTab(m_controlTab, QStringLiteral("Điều khiển"));
    m_tabs->addTab(m_recordTab, QStringLiteral("Ghi lưu"));
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
