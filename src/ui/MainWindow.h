#pragma once

#include "map/MapData.h"
#include "net/LinkConfig.h"
#include "proto/Asterix.h"
#include "proto/ScnCf.h"
#include "proto/ScnText.h"
#include "ui/ControlTab.h"
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
class PlaceholderPopup;
class PingService;
class PlotListWindow;
class RadarCenterPopup;
class RawIqStore;
class Recorder;
class Replayer;
class TrackStore;
class VqSender;
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
    void onFrame(quint32 category, quint32 serial, const QByteArray &data, bool be);
    void onScnStatus(const ScnText::Status &status);
    void onScnCf(const ScnCf::Message &message);
    void onAsterix(const Asterix::Batch &batch);
    void onPlot(const QByteArray &data, bool be);
    void applyVqConfig();
    // Đổi tâm đài: ghi setups.json, chiếu lại bản đồ, quỹ đạo, cấu hình SCH-VQ.
    void applyRadarCenter(double lat, double lon);
    void setCenterFromGps();
    void showScnStatus();
    void sendCmdAt();
    void sendCmdUser();
    void sendAdminCommand(quint32 category, const QVector<quint32> &fields);
    void sendRebootMh();
    void openEngineerWindow();
    void openViewIqWindow();
    void openPlotListWindow();

    void togglePopup(int id);
    void openPopup(int id);
    void closeCurrentPopup(int pendingId);
    void placePopup(int id);

    void toggleControlPanel();
    void setConnected(bool connected);
    // Xoá phần đang vẽ của dữ liệu (video, quỹ đạo, điểm dấu, trạng thái MH,
    // phản hồi của cửa sổ kỹ sư) khi dừng kết nối / bắt đầu, dừng phát lại.
    void clearDataView();
    // Các câu "đầu tiên" và bộ đếm của luồng SCN báo lại từ đầu.
    void resetSession();
    void setRecording(bool recording);
    void refreshRecordList();
    void setReplaying(bool replaying);
    void stopReplay();
    void onReplaySeeked();
    // Mở lại LinkManager chỉ với các dòng gửi được chọn ở nhóm "Phát lại".
    void startReplayLinks();
    void requestExit();

    MapData m_mapData;

    QSplitter *m_splitter = nullptr;
    MapView *m_mapView = nullptr;
    ControlPanel *m_controlPanel = nullptr;
    StatusPanel *m_statusPanel = nullptr;

    NotifyPopup *m_notifyPopup = nullptr;
    PlaceholderPopup *m_scnPopup = nullptr;
    NetworkPopup *m_networkPopup = nullptr;
    MhStatusPopup *m_mhPopup = nullptr;
    RadarCenterPopup *m_radarPopup = nullptr;
    QVector<SlidePopup *> m_popups;

    ColorSetupDialog *m_colorDialog = nullptr;
    EngineerWindow *m_engineerWindow = nullptr;
    // Tạo khi mở lần đầu: phần lớn phiên làm việc không ai vẽ cánh sóng.
    ViewIqWindow *m_viewIqWindow = nullptr;
    // Tạo sẵn: nhật ký điểm dấu MH ghi cả lúc cửa sổ đang đóng.
    PlotListWindow *m_plotWindow = nullptr;

    PingService *m_ping = nullptr;

    LinkConfig m_linkConfig;
    QString m_linkConfigError;
    QString m_linkConfigNote;
    LinkManager *m_links = nullptr;
    // Dùng chung giữa luồng nhận "Data-RAW" và cửa sổ ViewIQ; shared_ptr để
    // luồng nhận không bao giờ giữ con trỏ treo dù thứ tự huỷ thế nào.
    std::shared_ptr<RawIqStore> m_rawIq;
    // Ghi lưu sống suốt phiên chạy: bắt đầu / dừng được cả lúc đang kết nối.
    Recorder *m_recorder = nullptr;
    // Phát lại đi chung các slot nhận dữ liệu với LinkManager (onFrame…).
    Replayer *m_replayer = nullptr;
    bool m_replaying = false;
    // Giá trị điều khiển trước khi phát lại, trả lại khi dừng (step-07).
    ControlTab::Snapshot m_ctrlSnapshot{};
    QVector<QVector<quint32>> m_engineerSnapshot;
    bool m_gpsSaved = false;
    double m_gpsLatSaved = 0.0;
    double m_gpsLonSaved = 0.0;

    // Danh sách quỹ đạo (X18-VQ) và bộ dựng gói gửi VQ (SCH-VQ).
    TrackStore *m_tracks = nullptr;
    VqSender *m_vq = nullptr;
    bool m_vqTrackNoted = false;
    bool m_plotNoted = false;
    bool m_mergeNoted = false;
    bool m_mhTrackNoted = false;
    bool m_alarmNoted = false;
    // Trạng thái hộp "Khởi tạo quỹ đạo từ điểm dấu MH" lần áp dụng trước: bỏ
    // chọn thì phải xoá các quỹ đạo track_type 3.
    bool m_mhTrackInit = false;
    quint32 m_scnPlotsSent = 0;

    // Toạ độ GPS của gói STATUS_MH gần nhất cho nút "Đặt theo GPS". Giữ cả
    // sau khi dừng kết nối: tâm đài không đổi chỉ vì mất kết nối.
    bool m_hasGps = false;
    double m_gpsLat = 0.0;
    double m_gpsLon = 0.0;

    // Góc quét đến 400 lần/giây cho mỗi loại; thanh trạng thái chỉ cần 10 lần.
    QTimer *m_angleTimer = nullptr;
    double m_azRd = 0.0;
    double m_azMh = 0.0;
    bool m_hasAngles = false;

    // Luồng SCN: trạng thái phiên TCP với PC và gói Cf mới nhất PC gửi đến.
    ScnText::Status m_scnStatus;
    QString m_scnLastCf;
    quint32 m_scnCfCount = 0;

    QVariantAnimation *m_panelAnim = nullptr;
    int m_savedPanelWidth = 320;
    bool m_panelHidden = false;

    int m_openPopup = -1;
    int m_pendingPopup = -1;
    bool m_connected = false;
};
