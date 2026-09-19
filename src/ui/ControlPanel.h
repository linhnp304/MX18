#pragma once

#include <QWidget>

class AmplitudeView;
class ControlTab;
class SettingsTab;
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
    AmplitudeView *amplitudeView() const { return m_amplitude; }

private:
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_splitter = nullptr;
    SettingsTab *m_settingsTab = nullptr;
    ControlTab *m_controlTab = nullptr;
    AmplitudeView *m_amplitude = nullptr;
};
