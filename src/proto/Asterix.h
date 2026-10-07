#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

#include <optional>

// ASTERIX của hai luồng nối với hệ thống VQ: X18-VQ nhận CAT034/048 từ P18M,
// SCH-VQ gửi CAT034/048 cho VQ. Phân tích ở analysis-results/02-x18-vq.md và
// 03-sch-vq.md. Mọi số nhiều byte big-endian theo chuẩn — không theo khoá
// big_endian của connect.json.
//
// Khung: CAT (1) · LEN (2, tổng số byte cả khối) · rồi một hay nhiều bản ghi,
// mỗi bản ghi FSPEC (1+ byte, bit 1 là FX) · các item theo thứ tự FRN.
namespace Asterix {

constexpr quint8 kCat034 = 34;
constexpr quint8 kCat048 = 48;

// ------------------------------------------------------------------ giải mã

// CAT034 — thông báo dịch vụ radar. Chỉ giữ các item SW0 có đọc; item khác
// được nhảy qua đúng độ dài.
struct Cat034 {
    enum MessageType : quint8 { NorthMarker = 1, SectorCrossing, GeoFiltering, JammingStrobe };

    quint8 sac = 0, sic = 0;
    quint8 messageType = 0;
    std::optional<double> timeOfDay;        // giây từ nửa đêm UTC
    std::optional<quint8> sector;           // LSB 360/256 độ
    std::optional<double> rotationPeriod;   // giây
    std::optional<double> siteLat, siteLon; // I034/120, độ
    std::optional<int> siteHeight;          // mét
    // SP riêng của P18M (file 02 mục 3): byte trạng thái máy phát và máy hỏi.
    std::optional<quint8> p18c, nrz;
};

// SP kiểu ELM-2288 "PAROL" mà SCH-VQ gửi khi vq_output_p18m = false. Không có
// byte độ dài đầu nên chỉ giải mã được khi biết trước (DecodeOptions).
struct ParolIff {
    quint8 rm = 0;
    bool friendly = false, commander = false;
    std::optional<double> rangeM, azimuthDeg;
    std::optional<quint8> fuel;
    std::optional<double> heightM;
    std::optional<quint32> id;
};

// CAT048 — báo cáo mục tiêu. Có Track Number là quỹ đạo, không có là điểm dấu.
struct Cat048 {
    quint8 sac = 0, sic = 0;
    std::optional<double> timeOfDay;

    // I048/020
    quint8 typ = 0;
    bool sim = false, rdp = false, spi = false, rab = false;
    bool tst = false, me = false, mi = false;
    quint8 foeFri = 0;

    std::optional<double> rangeM, azimuthDeg;   // I048/040
    std::optional<quint16> mode3a;              // I048/070, 12 bit (in bát phân)
    bool mode3aV = false, mode3aG = false, mode3aL = false;
    std::optional<double> flightLevel;          // I048/090, FL
    std::optional<quint32> aircraftAddress;     // I048/220
    std::optional<quint16> trackNumber;         // I048/161
    std::optional<double> x, y;                 // I048/042, mét, x Đông y Bắc
    std::optional<double> speed, heading;       // I048/200, m/s và độ

    // I048/170
    bool hasStatus = false;
    bool cnf = false, dou = false, mah = false;
    quint8 rad = 0, cdm = 0;
    bool tre = false, gho = false, sup = false, tcc = false;

    std::optional<double> heightM;              // I048/110
    // SP/RE dạng "04 80 NRZ1 NRZ2" của P18M (FRN27 hoặc FRN28, chưa chốt).
    std::optional<quint8> nrz1, nrz2;
    std::optional<ParolIff> parol;

    bool isTrack() const { return trackNumber.has_value(); }
};

struct DecodeOptions {
    // FRN27 của CAT048 là SP kiểu ELM-2288 chứ không phải SP chuẩn có byte độ
    // dài — chỉ dùng khi đọc lại chính gói SCH-VQ (công cụ kiểm thử).
    bool parolSp = false;
};

struct Batch {
    QVector<Cat034> services;
    QVector<Cat048> reports;
    int otherBlocks = 0;      // khối CAT khác 34/48: bỏ qua
};

// Giải mã một datagram: nhiều khối, mỗi khối nhiều bản ghi. Gặp chỗ hỏng thì
// dừng ở đó, giữ những bản ghi đã đọc được và trả false kèm lý do trong *error.
bool decode(const QByteArray &datagram, Batch *out, QString *error = nullptr,
            const DecodeOptions &options = {});

// ------------------------------------------------------------------ mã hoá

struct EncodeConfig {
    quint8 sac = 148;
    quint8 sic = 101;
    // Hệ số k của cự ly (SW0 "VQRangeChange"): 2,0 là LSB chuẩn ASTERIX.
    double rangeChange = 2.0;
    // true: SP kiểu P18M "04 80 NRZ1 NRZ2"; false: SP kiểu ELM-2288 "PAROL".
    bool outputP18m = false;
};

struct Site {
    double lat = 21.0;
    double lon = 105.0;
    int heightM = 15;
};

// Time of Day đúng chuẩn: giây từ nửa đêm UTC × 128, 24 bit. SW0 nhân Ticks
// của .NET nên tràn số và gửi giờ sai gốc (file 03 mục 3).
quint32 timeOfDay(const QDateTime &utc);
quint32 timeOfDayNow();

QByteArray northMarker(const EncodeConfig &cfg, quint32 tod, double periodS, const Site &site);
QByteArray sectorCrossing(const EncodeConfig &cfg, quint32 tod, quint8 sector);
QByteArray jammingStrobe(const EncodeConfig &cfg, quint32 tod, double azimuthDeg);

// Một bản ghi CAT048 gửi VQ. Bốn kiểu theo isTrack × iff (file 03 mục 5).
struct TargetOut {
    bool isTrack = true;
    bool iff = false;
    quint16 trackNumber = 0;    // chỉ 12 bit thấp được gửi
    double rangeM = 0.0;
    double azimuthDeg = 0.0;
    double speedMps = 0.0;      // chỉ quỹ đạo
    double headingDeg = 0.0;
    double heightM = 0.0;
    // Bản tin cuối của quỹ đạo (I048/170 TRE = 1) — anh Linh chốt MX18 gửi khi
    // xoá quỹ đạo; SW0 không bao giờ gửi.
    bool endOfTrack = false;

    // Nhận dạng của gói IFF (SP kiểu PAROL).
    quint8 returnedMode = 0;
    bool commander = false;
    quint32 flightId = 0;
    quint8 fuel = 0;
    quint8 nrz1 = 0, nrz2 = 0;  // SP kiểu P18M; SW0 chưa bao giờ gán nên luôn 0
};

QByteArray targetReport(const EncodeConfig &cfg, quint32 tod, const TargetOut &t);

// Một dòng mô tả để ghi nhật ký / thông báo.
QString describe(const Cat034 &m);
QString describe(const Cat048 &m);

} // namespace Asterix

Q_DECLARE_METATYPE(Asterix::Batch)
