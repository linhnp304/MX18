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
//   records.json     - lựa chọn ghi lưu (đọc lại mỗi lần bắt đầu ghi lưu)
//
// Quy tắc chung: thiếu file thì tạo mặc định, có file mà đọc lỗi thì giữ giá trị
// mặc định và đẩy một dòng [Lỗi] vào "Thông báo hệ thống" (xem takeLoadErrors).

struct SwInfo {
    QString line0 = QStringLiteral("MX18 (V2026)");                       // màn hình giới thiệu
    QString line1 = QStringLiteral("PHẦN MỀM TRẮC THỦ RA ĐA MX18");
    QString line2 = QStringLiteral("Phiên bản: 1.0.0926");
};

struct DisplayColors {
    QColor grid         = QColor(0xFF, 0xFF, 0x80); // đường quét + đường chia độ
    QColor trackTrail   = QColor(0xFF, 0xA5, 0x00); // vết lịch sử quỹ đạo
    QColor track        = QColor(0x3F, 0xA9, 0xF5); // quỹ đạo chưa có nhận dạng (track_type 1)
    QColor trackMh      = QColor(0xFF, 0x00, 0x00); // quỹ đạo có nhận dạng MH (track_type 2, 3)
    QColor trackProfile = QColor(0xFF, 0xFF, 0xC8); // lý lịch quỹ đạo
    QColor plot         = QColor(0xFF, 0x30, 0x30); // điểm dấu MH
    QColor alarm1       = QColor(0xB0, 0x40, 0xFF); // tia báo động nhấp nháy giữa hai màu
    QColor alarm2       = QColor(0xFF, 0x20, 0x20);
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
    int plotHoldSec = 8;       // giây hiển thị điểm dấu MH (và tia báo động), 1..60
    int trackDropSec = 40;     // giây không có cập nhật thì xoá quỹ đạo, 10..600
    int trackSizePct = 100;
    int plotSizePct = 100;
    // false: hợp nhất điểm dấu MH vào quỹ đạo X18-VQ (phương án 1);
    // true: khởi tạo và bám quỹ đạo từ điểm dấu MH (phương án 2).
    bool mhTrackInit = false;
    double mergeAzimuthDeg = 3.0;  // cửa sổ hợp nhất: ± ngần này quanh điểm dấu
    double mergeRangeKm = 3.0;

    double radarLat = 21.202111;
    double radarLon = 105.813417;
};

// Hệ số hiệu chỉnh một trường STATUS_MH (step-07): giá trị hiển thị =
// [giá trị theo giao thức gốc] / StDiv + StAdd, rồi mới so với ngưỡng.
struct StatusScale {
    double stDiv = 1.0;
    double stAdd = 0.0;
    double apply(double v) const { return v / stDiv + stAdd; }
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

    // Hệ số theo tên trường của gói, mặc định anh Linh đưa trong step-07. Ba
    // mức nguồn vẫn chia 10 (-10) theo giao thức gốc trước khi áp hệ số.
    StatusScale k2Nguon50V{3.682, 0.0};
    StatusScale k2Nguon5V{16.84, 0.0};
    StatusScale k2NguonM5V{10.2, 0.0};
    StatusScale k5Tx1Cs{1.0, -127.0};
    StatusScale k5Tx1Hssd{1.0, -127.0};
    StatusScale k6Tx2Cs{1.0, -127.0};
    StatusScale k6Tx2Hssd{1.0, -127.0};
};

// Luồng SCH-VQ (gửi ASTERIX cho VQ) — khoá trong setupadmin.json, chưa có giao
// diện (anh Linh chốt ở giai đoạn 5, analysis-results/04 mục 4).
struct VqSetup {
    double rangeChange = 2.0;          // vq_range_change: hệ số k, 2,0 = LSB chuẩn
    bool outputP18m = false;           // vq_output_p18m: SP kiểu P18M thay vì ELM-2288
    int sac = 148;                     // vq_sac
    int sic = 101;                     // vq_sic
    // vq_sector_source: góc anten cho North marker / Sector crossing,
    // "VIDEO_R" hoặc "VIDEO_I".
    bool sectorFromVideoI = false;
    bool sendTre = true;               // vq_send_tre: bản tin cuối khi xoá quỹ đạo
    int siteHeightM = 15;              // vq_site_height_m: độ cao đài trong North marker
};

// Bộ bám quỹ đạo từ điểm dấu MH — khoá trong setupadmin.json để kỹ sư chỉnh,
// chưa có giao diện (step-06 mục 9).
struct MhTrackerSetup {
    int initScans = 2;                 // mh_init_scans: số vòng liên tiếp để khởi tạo
    double speedMinMps = 10.0;         // mh_speed_min_mps
    double speedMaxMps = 333.3;        // mh_speed_max_mps
    int extrapolateScans = 3;          // mh_extrapolate_scans
    double scanPeriodS = 10.0;         // mh_scan_period_s: trước khi đo được chu kỳ quét
    double windowAzimuthDeg = 3.0;     // mh_window_azimuth_deg: cửa sổ dự đoán
    double windowRangeKm = 3.0;        // mh_window_range_km
    // mh_track_id_start: khác dải số hiệu của P18M; Track Number CAT048 chỉ 12
    // bit nên quay vòng trong [giá trị này, 4095].
    int trackIdStart = 3001;
};

// Ghi lưu (./settings/records.json, step-07). Chưa có giao diện: kỹ sư sửa
// file, lần bấm "Bắt đầu ghi lưu" kế tiếp có tác dụng, không phải chạy lại.
// Dung lượng tính theo GB nhị phân (1 GB = 1024³ byte) như MB trên tab "Ghi lưu".
struct RecordSetup {
    bool fullVideoR = false;           // write_full_video_r: false = chỉ 24 byte đầu
    bool fullVideoI = true;            // write_full_video_i
    bool rawIq = false;                // write_raw_iq: RAW_IQ ~7,7 MB/s, anh Linh chốt không ghi
    double maxSizeGb = 2.0;            // write_max_size: ngắt sang file mới
    double maxTimeH = 2.0;             // write_max_time
    double totalCapGb = 500.0;         // total_cap: tổng ./records, quá thì xoá file cũ
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
    const VqSetup &vq() const { return m_vq; }
    const MhTrackerSetup &mhTracker() const { return m_mhTracker; }
    const RecordSetup &records() const { return m_records; }
    // Đọc lại records.json; lỗi vào takeLoadErrors() như lúc khởi động.
    void loadRecords();

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
    VqSetup m_vq;
    MhTrackerSetup m_mhTracker;
    RecordSetup m_records;
    QStringList m_loadErrors;
};
