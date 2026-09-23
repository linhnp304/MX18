#pragma once

#include "map/MapData.h"
#include "net/LinkConfig.h"
#include "ui/StatusPanel.h"

#include <QMainWindow>
#include <QVector>

#include <memory>

class ColorSetupDialog;
class ControlPanel;
class EngineerWindow;
class LinkManager;
class MapView;
class MhStatusPopup;
class NetworkPopup;
class NotifyPopup;
class PingService;
class RadarCenterPopup;
class RawIqStore;
class ViewIqWindow;
class SlidePopup;
class QSplitter;
class QTimer;
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
    void reportConfigErrors();

    // Phân loại gói tin nhận được về đúng nơi hiển thị.
    void onFrame(quint32 category, quint32 serial, const QByteArray &data);
    void sendCmdAt();
    void sendCmdUser();
    void sendAdminCommand(quint32 category, const QVector<quint32> &fields);
    void sendRebootMh();
    void openEngineerWindow();
    void openViewIqWindow();

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
    MhStatusPopup *m_mhPopup = nullptr;
    RadarCenterPopup *m_radarPopup = nullptr;
    QVector<SlidePopup *> m_popups;

    ColorSetupDialog *m_colorDialog = nullptr;
    EngineerWindow *m_engineerWindow = nullptr;
    // Tạo khi mở lần đầu: phần lớn phiên làm việc không ai vẽ cánh sóng.
    ViewIqWindow *m_viewIqWindow = nullptr;

    PingService *m_ping = nullptr;

    LinkConfig m_linkConfig;
    QString m_linkConfigError;
    LinkManager *m_links = nullptr;
    // Dùng chung giữa luồng nhận "Data-RAW" và cửa sổ ViewIQ; shared_ptr để
    // luồng nhận không bao giờ giữ con trỏ treo dù thứ tự huỷ thế nào.
    std::shared_ptr<RawIqStore> m_rawIq;

    // Góc quét đến 400 lần/giây cho mỗi loại; thanh trạng thái chỉ cần 10 lần.
    QTimer *m_angleTimer = nullptr;
    double m_azRd = 0.0;
    double m_azMh = 0.0;
    bool m_hasAngles = false;

    QVariantAnimation *m_panelAnim = nullptr;
    int m_savedPanelWidth = 320;
    bool m_panelHidden = false;

    int m_openPopup = -1;
    int m_pendingPopup = -1;
    bool m_connected = false;
};
