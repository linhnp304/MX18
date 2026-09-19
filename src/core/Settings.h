#pragma once

#include <QColor>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// Mỗi nhóm cấu hình một file trong ./settings để anh Linh sửa tay được:
//   swinfo.json      - chữ trên màn hình giới thiệu và ô thông tin phần mềm
//   setups.json      - lựa chọn hiển thị của trắc thủ, ghi lại mỗi lần đổi
//   checkip.json     - danh sách nút mạng cần ping
//   setupadmin.json  - thiết lập cửa sổ mức kỹ sư, kể cả mật khẩu
//   statuserror.json - ngưỡng báo lỗi của các giá trị trạng thái MH
//   params.json      - tham số đài (tab "Params", giai đoạn sau)
//   connect.json     - cấu hình cổng gửi/nhận (xem net/LinkConfig.h)
//
// Quy tắc chung: thiếu file thì tạo mặc định, có file mà đọc lỗi thì giữ giá trị
// mặc định và đẩy một dòng [Lỗi] vào "Thông báo hệ thống" (xem takeLoadErrors).

struct SwInfo {
    QString line0 = QStringLiteral("MX18 (V2026)");                       // màn hình giới thiệu
    QString line1 = QStringLiteral("PHẦN MỀM TRẮC THỦ RA ĐA MX18");
    QString line2 = QStringLiteral("Phiên bản: 1.0.0926");
};

struct DisplayColors {
    QColor grid       = QColor(0xFF, 0xFF, 0x80); // đường quét + đường chia độ
    QColor trackTrail = QColor(0xFF, 0xA5, 0x00); // vết lịch sử quỹ đạo
    QColor track      = QColor(0x3F, 0xA9, 0xF5); // quỹ đạo
    QColor plot       = QColor(0xFF, 0x30, 0x30); // điểm dấu MH
};

struct Setups {
    bool showMap = true;
    bool showAirRoutes = true;
    bool showAirports = true;
    int brightness = 3;        // 1..10
    int videoFade = 5;         // 0..10
    int trackHistory = 10;     // 0..100
    int trailStyle = 0;        // 0: điểm, 1: đường
    bool showTrackProfile = false;
    bool showPlotInfo = false;
    int rangeRingMode = 0;     // 0: 50km, 1: 10km, 2: 5km, 3: tắt
    int azimuthMode = 0;       // 0: 30 độ, 1: 10 độ, 2: 5 độ, 3: tắt

    DisplayColors colors;
    int plotHoldSec = 8;       // giây hiển thị điểm dấu MH
    int trackSizePct = 100;
    int plotSizePct = 100;

    double radarLat = 21.202111;
    double radarLon = 105.813417;
};

// Ngưỡng báo lỗi cho cửa sổ "Trạng thái MH" (./settings/statuserror.json).
struct StatusLimits {
    double min50V = 44.0;
    double max50V = 56.0;
    double min5V  = 4.4;
    double max5V  = 5.6;
    int minCs = 60;     // công suất phát tối thiểu khi đang nối phát
    int maxT  = 90;     // nhiệt độ tối đa
    int maxH  = 99;     // độ ẩm tối đa
};

struct NetNode {
    QString name;
    QString address;
    int kind = 0;              // 0: không cảnh báo, 1: cảnh báo, 2: báo lỗi
};

// Cự ly tối đa của đài, dùng cho vòng cự ly ngoài cùng và khung nhìn mặc định.
constexpr double kMaxRangeKm = 360.0;

class Settings : public QObject
{
    Q_OBJECT
public:
    static Settings &instance();

    void load();                 // đọc cả bốn file, thiếu thì tạo mặc định

    SwInfo &swInfo() { return m_swInfo; }
    Setups &setups() { return m_setups; }
    const StatusLimits &statusLimits() const { return m_limits; }
    const QVector<NetNode> &netNodes() const { return m_netNodes; }
    void setNetNodes(const QVector<NetNode> &nodes);

    QString engineerPassword() const { return m_engineerPassword; }

    // Ô "Khóa điều khiển" ở góc dưới cửa sổ mức kỹ sư.
    bool adminLocked() const { return m_adminLocked; }
    void setAdminLocked(bool locked);

    void saveSetups();
    void saveSwInfo();
    void saveSetupAdmin();
    void saveNetNodes();

    // Lỗi đọc cấu hình gom lại lúc load(); MainWindow lấy ra để hiện [Lỗi]
    // sau khi cửa sổ thông báo đã dựng xong.
    QStringList takeLoadErrors();

signals:
    void setupsChanged();        // phát sau mỗi lần trắc thủ đổi lựa chọn

public:
    void notifyChanged() { saveSetups(); emit setupsChanged(); }

private:
    Settings() = default;

    void loadSwInfo();
    void loadSetups();
    void loadNetNodes();
    void loadSetupAdmin();
    void loadStatusLimits();

    // Đọc một file cấu hình, ghi lại lỗi cú pháp vào m_loadErrors.
    QJsonObject readChecked(const QString &path);

    SwInfo m_swInfo;
    Setups m_setups;
    StatusLimits m_limits;
    QVector<NetNode> m_netNodes;
    QString m_engineerPassword = QStringLiteral("X18");
    bool m_adminLocked = true;
    QStringList m_loadErrors;
};
