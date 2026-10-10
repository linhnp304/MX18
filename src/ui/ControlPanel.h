#pragma once

#include <QWidget>

class AmplitudeView;
class ControlTab;
class RecordTab;
class SettingsTab;
class TrackListTab;
class QSplitter;
class QTabWidget;

// Panel 2: các tab điều khiển (80% chiều dọc) và cửa sổ biên độ (20%).
class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanel(QWidget *parent = nullptr);

    SettingsTab *settingsTab() const { return m_settingsTab; }
    ControlTab *controlTab() const { return m_controlTab; }
    TrackListTab *trackListTab() const { return m_trackListTab; }
    RecordTab *recordTab() const { return m_recordTab; }
    AmplitudeView *amplitudeView() const { return m_amplitude; }

private:
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_splitter = nullptr;
    SettingsTab *m_settingsTab = nullptr;
    ControlTab *m_controlTab = nullptr;
    TrackListTab *m_trackListTab = nullptr;
    RecordTab *m_recordTab = nullptr;
    AmplitudeView *m_amplitude = nullptr;
};
