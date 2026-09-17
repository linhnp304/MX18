#pragma once

#include "map/MapData.h"
#include "ui/StatusPanel.h"

#include <QMainWindow>
#include <QVector>

class ColorSetupDialog;
class ControlPanel;
class MapView;
class NetworkPopup;
class NotifyPopup;
class PingService;
class RadarCenterPopup;
class SlidePopup;
class QSplitter;
class QVariantAnimation;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Nạp nền bản đồ số; trả về false nếu thiếu dữ liệu (vẫn chạy được).
    bool loadMapData();

    void notify(const QString &message, bool isError = false);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void wireSignals();
    void startPing();

    void togglePopup(int id);
    void openPopup(int id);
    void closeCurrentPopup(int pendingId);
    void placePopup(int id);

    void toggleControlPanel();
    void setConnected(bool connected);
    void requestExit();

    MapData m_mapData;

    QSplitter *m_splitter = nullptr;
    MapView *m_mapView = nullptr;
    ControlPanel *m_controlPanel = nullptr;
    StatusPanel *m_statusPanel = nullptr;

    NotifyPopup *m_notifyPopup = nullptr;
    NetworkPopup *m_networkPopup = nullptr;
    RadarCenterPopup *m_radarPopup = nullptr;
    QVector<SlidePopup *> m_popups;

    ColorSetupDialog *m_colorDialog = nullptr;

    PingService *m_ping = nullptr;

    QVariantAnimation *m_panelAnim = nullptr;
    int m_savedPanelWidth = 320;
    bool m_panelHidden = false;

    int m_openPopup = -1;
    int m_pendingPopup = -1;
    bool m_connected = false;
};
