#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;

// Tab "Cài đặt" của panel 2: mọi tuỳ chọn hiển thị của trắc thủ.
// Thay đổi ở bất kỳ control nào đều ghi ngay vào ./settings/setups.json.
class SettingsTab : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsTab(QWidget *parent = nullptr);

    void setConnected(bool connected);
    bool isConnected() const { return m_connected; }

signals:
    void displayChanged();       // panel 1 phải vẽ lại
    void colorSetupRequested();
    void connectToggled(bool connected);
    void exitRequested();

private:
    void buildUi();
    void loadFromSettings();
    void pushToSettings();

    QCheckBox *m_showMap = nullptr;
    QCheckBox *m_showAirRoutes = nullptr;
    QCheckBox *m_showAirports = nullptr;
    QSlider *m_brightness = nullptr;
    QLabel *m_brightnessValue = nullptr;
    QSlider *m_videoFade = nullptr;
    QLabel *m_videoFadeValue = nullptr;
    QSlider *m_trackHistory = nullptr;
    QLabel *m_trackHistoryValue = nullptr;
    QRadioButton *m_trailDot = nullptr;
    QRadioButton *m_trailLine = nullptr;
    QCheckBox *m_showTrackProfile = nullptr;
    QCheckBox *m_showPlotInfo = nullptr;
    QRadioButton *m_ring[4] = {nullptr, nullptr, nullptr, nullptr};
    QRadioButton *m_azimuth[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton *m_colorSetupBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_exitBtn = nullptr;

    bool m_connected = false;
    bool m_loading = false;
};
