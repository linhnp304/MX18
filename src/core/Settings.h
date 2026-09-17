#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>

// Bốn nhóm cấu hình, mỗi nhóm một file trong ./settings để anh Linh sửa tay được:
//   swinfo.json  - chữ trên màn hình giới thiệu và ô thông tin phần mềm
//   setups.json  - lựa chọn hiển thị của trắc thủ, ghi lại mỗi lần đổi
//   checkip.json - danh sách nút mạng cần ping
//   params.json  - tham số mức kỹ sư (kể cả mật khẩu)

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
    const QVector<NetNode> &netNodes() const { return m_netNodes; }

    QString engineerPassword() const { return m_engineerPassword; }
    void setEngineerPassword(const QString &pw);

    void saveSetups();
    void saveSwInfo();
    void saveParams();

signals:
    void setupsChanged();        // phát sau mỗi lần trắc thủ đổi lựa chọn

public:
    void notifyChanged() { saveSetups(); emit setupsChanged(); }

private:
    Settings() = default;

    void loadSwInfo();
    void loadSetups();
    void loadNetNodes();
    void loadParams();

    SwInfo m_swInfo;
    Setups m_setups;
    QVector<NetNode> m_netNodes;
    QString m_engineerPassword = QStringLiteral("X18");
};
