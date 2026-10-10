#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

// Danh sách trường của từng gói tin, đúng thứ tự trong docs/step-02.md.
//
// Mỗi gói mô tả bằng một enum chỉ số + mảng quint32 chứ không phải struct có tên
// trường: giao diện điều khiển và cửa sổ trạng thái đều duyệt theo bảng, nhờ vậy
// thêm/bớt một trường chỉ phải sửa một chỗ.

namespace CmdAt {

enum Field {
    AntenOnoff = 0,   // 0 dừng, 1 quay
    AntenSpeed,       // 1..6 vòng/phút
    AntenSync,        // 0 độc lập, 1 đồng bộ
    Reserved1, Reserved2, Reserved3, Reserved4, Reserved5,
    Count
};

const quint32 *defaults();

} // namespace CmdAt

namespace CmdUser {

enum Field {
    CdLamviec = 0,    // 0 tạo giả, 1 làm việc, 2 TLKT
    GiaBd,            // giả báo động
    GiaBn,            // giả báo nạn
    Xungkich,         // 0 ngoài, 1 trong — chưa có điều khiển trên giao diện
    NguonPvi,         // 0 giả quay, 1 encoder
    VantocGiaquay,    // 0: 6 v/p, 1: 12 v/p
    NguonCs,          // 0 tắt, 100 bật
    Icode1,           // 0 máy bay, 1 tàu biển
    Mode,             // 1..5 (icode1=1) hoặc 1..4,6 (icode1=0)
    Rcode1,           // 1..12
    Icode3,           // 1..6
    Rcode3,           // 1..7
    KeyM2,            // 0 TĐ, 1 HT, 2 KT, 3 HT+KT, 4 xóa khóa
    Noiphat,
    Kenhphu,
    CsPhat,           // 0: 50%, 1: 100%
    CdPhat,           // 0 liên tục, 1 rẻ quạt, 2 ngắt 1v, 3 ngắt 2v
    Azm1, Azm2,       // rẻ quạt 1: phương vị đầu / cuối
    Azm3, Azm4,       // rẻ quạt 2
    CnKdb,
    CnAk,
    HsAk,
    Monopulse,
    NguongMonopulse,
    NguongXungdon,
    HsStc,            // 0..3
    NguongMonopulse2,
    NguongXungdon2,
    Reserved1, Reserved2, Reserved3,
    Count
};

const quint32 *defaults();

} // namespace CmdUser

namespace StatusMh {

enum Field {
    K2Nguon50V = 0,
    K2Nguon5V,
    K2NguonM5V,
    K2CtrBack,        // chưa dùng đến
    K6Tx2Cs,
    K6Tx2Hssd,
    K6Tx2Nhietdo,
    K6Tx2Doam,
    K6StcBack,
    K6CtrBack,
    K5Tx1Cs,
    K5Tx1Hssd,
    K5Tx1Nhietdo,
    K5Tx1Doam,
    K5StcBack,
    K5CtrBack,
    K3BetaBack,
    K3Key,
    K3Nhietdo,
    K3Doam,
    Giatribu,
    K2Nhietdo,
    K2Doam,
    KeyTime,
    GpsStatus,
    GpsLat,
    GpsLng,
    Count
};

// "yy/MM/dd HH:mm:ss" giải mã từ trường KeyTime đóng gói bit.
QString keyTimeText(quint32 keyTime);

// Hướng GPS (độ, 0..360) lấy 12 bit thấp của GpsStatus.
double gpsHeading(quint32 gpsStatus);

// true khi cả ba bit tín hiệu / toạ độ / vi sai đều tốt.
bool gpsFixGood(quint32 gpsStatus);

} // namespace StatusMh

// ------------------------------------------------- lệnh điều khiển mức kỹ sư

namespace CmdAdmin {

enum Field {
    CuongdoVideo = 0,   // 0..31
    DiemdauVideo,       // 0..315
    KenhVideo,          // 0..3: SumF2 / SubF2 / SumF3 / SubF3
    CuasoNguong,        // 0 tắt, 1 bật
    Deltatx,            // bù K2 Sys-F4: raw = round(GUI * 1000)
    Tx1Phase100, Tx1Phase50,
    Tx1Amp100, Tx1Amp50,
    Tx2Phase100, Tx2Phase50,
    Tx2Amp100, Tx2Amp50,
    DoCs,               // 0 tắt, 1 bật
    ViewIq,             // 0..7
    CalibOnoff,         // 0..8
    // Năm trường bù pha/biên độ nằm trong gói nhưng điều khiển của chúng ở gói
    // CMD_ADMIN_BUPHABD (tab "Other"); ở đây chỉ giữ chỗ cho đúng thứ tự byte.
    CalibBuF2, CalibBuF4, CalibBuF2Amp, CalibBuF3Amp, CalibBuF4Amp,
    AkTest,             // 0 tắt, 1 bật
    AkGainKc, AkGainKp, AkGainKt,
    DeltatxF2, DeltatxF3,   // raw = round(GUI * 1000)
    Reserved1, Reserved2, Reserved3,
    Count
};

const quint32 *defaults();

} // namespace CmdAdmin

namespace CmdAdminAd {

enum Field {
    Ftx = 0,            // MHz hoặc KHz tuỳ InputType
    Frx,
    GainRx1, GainRx2,   // 0..70
    GainTx1, GainTx2,   // 0..85
    InputType,          // 0 MHz, 1 KHz
    Reserved1, Reserved2,
    Count
};

const quint32 *defaults();

// Bảng "Chọn tần số" của tab "AD": mỗi mục đặt sẵn một cặp tần số phát/thu.
struct FreqPreset {
    const char *name;
    quint32 ftx;
    quint32 frx;        // 0 = giữ nguyên giá trị đang có (mục "TLKT")
};

int freqPresetCount();
const FreqPreset &freqPreset(int index);

} // namespace CmdAdminAd

namespace CmdAdminSw {

enum Field {
    VideoMulti = 0,     // 1..1000000
    VideoDivi,          // 1..1000000
    CxMin, CxMax,       // 2..200
    CxBegin,            // 2..10
    CxEnd,              // 2..100
    EnaPlotDebug,       // 0..3
    EnaVideoSrc,        // 0..4
    EnaPlotSrc,         // 0,1
    StTimer,            // 50..500
    EnaPrintConsole,    // 0..8
    IqSrc,              // dự phòng, gán 0
    AutoBugps,          // 0,1
    SvrDeltaRange, SvrDeltaBeta,   // dự phòng, gán 0
    Reserved1,
    Count
};

const quint32 *defaults();

} // namespace CmdAdminSw

namespace CmdAdminOther {

enum Field {
    BuGoc = 0,          // 0 tắt, 1 bù từ GPS, 2 bù bằng tay
    GiatriBu,           // 0..4095: raw = round(GUI * 4096 / 360), 4096 quy về 0
    PlotBuCly,          // 0..360000 mét
    PlotBuPvi,          // raw = round(GUI * 100), có dấu
    Locxung,            // 0..65535
    LuuThamso,          // 0,1
    CalibRM2,           // có dấu, mét
    Reserved1, Reserved2, Reserved3, Reserved4, Reserved5,
    Count
};

const quint32 *defaults();

} // namespace CmdAdminOther

namespace CmdAdminCalibReg {

// Mỗi cặp (re, im) là một số phức: re = round(GUI * 32768), im tính theo radian
// nên nhập bằng độ rồi đổi: im = round(GUI / 180 * PI * 32768).
enum Field {
    F21Re = 0, F21Im, F22Re, F22Im,
    F31Re, F31Im, F32Re, F32Im,
    F41Re, F41Im, F42Re, F42Im,
    Reserved1, Reserved2, Reserved3, Reserved4, Reserved5,
    Count
};

const quint32 *defaults();

} // namespace CmdAdminCalibReg

namespace CmdAdminBuphabd {

enum Field {
    BuF2 = 0, BuF3, BuF4,          // -359..359 độ, có dấu
    BuF2Amp, BuF3Amp, BuF4Amp,     // 1..2000
    Reserved1, Reserved2, Reserved3, Reserved4, Reserved5,
    Count
};

const quint32 *defaults();

} // namespace CmdAdminBuphabd

namespace StatusCalib {

// calib_read[6] rồi calib_feedback[28] rồi tx2_amp1, tx2_amp2 và 3 trường dự phòng.
constexpr int kReadCount = 6;
constexpr int kFeedbackCount = 28;

enum Field {
    Read0 = 0,
    Feedback0 = kReadCount,
    Tx2Amp1 = Feedback0 + kFeedbackCount,
    Tx2Amp2,
    Reserved1, Reserved2, Reserved3,
    Count
};

} // namespace StatusCalib

namespace StatusParams {

constexpr int kCount = 100;

// Tên tham số của xparams[i]; "NA" với các ô chưa dùng đến.
const char *name(int index);

} // namespace StatusParams

// ------------------------------------------------ giai đoạn 6: mục tiêu

// Điểm dấu MH (0x2031) nhận trên "Data-Status".
namespace Plot {

enum Field {
    Azm = 0,          // 0..35999, 0,01 độ
    Range,            // mét
    Retmode,          // 1..9 chế độ phản hồi
    Commander,        // 0,1 — chỉ có ở retmode 3
    Flightid,         // số hiệu, 0 = bỏ qua — chỉ có ở retmode 4
    Altitude,         // mét, 0 = bỏ qua — chỉ có ở retmode 6
    Fuellevel,        // 0..99 %, 0 = bỏ qua — chỉ có ở retmode 6
    CxNum,            // số xung trong chùm
    CxAzmStart, CxAzmEnd,   // phương vị encoder 0..4095 đầu / cuối chùm
    CxRangeStart,     // ô cự ly đầu chùm
    Reserved1, Reserved2, Reserved3,
    Count
};

// Trường bắt buộc phải có để đặt được điểm dấu. Quy tắc nhận của step-07: gói
// cụt lấy được trường nào hay trường đó, phần thiếu giữ mặc định của defaults().
constexpr int kMinCount = Range + 1;

const quint32 *defaults();

} // namespace Plot

// Quỹ đạo (0x2051): không đi trên mạng, là bản ghi của danh sách quỹ đạo.
namespace Track {

enum Field {
    TrackType = 0,    // Type
    TrackStatus,      // Status
    TrackId,          // khoá quản lý danh sách
    TrackTop,         // số đầu tốp hiển thị, ban đầu = track_id
    Azm,              // 0..35999, 0,01 độ
    Range,            // mét
    Velocity,         // m/s
    Heading,          // 0..35999, 0,01 độ
    IffReturnedMode,  // 0..9, 0 = chưa có nhận dạng
    IffCommander,
    IffFlightid,
    IffAltitude,      // mét
    IffFuellevel,     // %
    Lat, Lng,         // float IEEE 754 32 bit, độ
    Reserved01, Reserved02, Reserved03, Reserved04,
    WindowAzm1, WindowAzm2,       // cửa sổ dự đoán, 0,01 độ
    WindowRange1, WindowRange2,   // cửa sổ dự đoán, mét
    Reserved11, Reserved12, Reserved13, Reserved14,
    Reserved21, Reserved22, Reserved23, Reserved24, Reserved25,
    Count
};

enum Type : quint32 {
    TypeVq = 1,       // nhận từ X18-VQ
    TypeVqMh = 2,     // nhận từ X18-VQ, đã hợp nhất điểm dấu MH
    TypeMh = 3,       // khởi tạo từ điểm dấu MH
};

enum Status : quint32 {
    StatusInit = 1, StatusConfirmed, StatusTracking, StatusLost, StatusExtrapolated, StatusDeleted,
};

const quint32 *defaults();

// lat/lng nằm trong mảng trường dưới dạng bit của số float.
double latLng(quint32 raw);
quint32 rawLatLng(double deg);

} // namespace Track

// Hướng có báo động (0x7720) nhận trên "Data-Status".
namespace AlarmHead {

enum Field {
    Head = 0,         // 0..35999, 0,01 độ
    Count
};

} // namespace AlarmHead

// ------------------------------------- giai đoạn 7: gói nhận, chưa dùng tới
// Anh Linh tra tài liệu và đối chiếu file 20261008_02.pcapng: MX18 nhận về nhưng
// chưa hiển thị (thông tin GPS đã có trong STATUS_MH); giữ sẵn giao thức.

// Góc anten (0x2032), khoảng 16 × 2,5 ms một gói, có checksum.
namespace SectorI {

enum Field {
    Sector = 0,       // 0..35999, 0,01 độ
    Count
};

} // namespace SectorI

// GPS riêng (0x6020): 40 byte = 5 trường đầu + 5 trường, không checksum.
namespace GpsData {

enum Field {
    Heading = 0,      // 0..35999, 0,01 độ — chỉ đúng khi Status = 2
    Latitude,         // độ × 10000
    Longitude,        // độ × 10000
    Altitude,         // mét
    Status,           // 0 mất tín hiệu, 1 có lat/lng, 2 có cả hướng
    Count
};

struct Info {
    double headingDeg = 0.0;
    double lat = 0.0;
    double lon = 0.0;
    int altitudeM = 0;
    int status = 0;
};

Info decode(const quint32 *fields);

} // namespace GpsData

// Ngày giờ build firmware MH (0x99810): 48 byte = 5 + 6 trường + checksum.
namespace MhFwVersion {

enum Field {
    Year = 0, Month, Day, Hour, Minute, Second,
    Count
};

// "yyyy-MM-dd HH:mm:ss", rỗng nếu ngày giờ vô nghĩa.
QString text(const quint32 *fields);

} // namespace MhFwVersion

// Đường quét: azimuth 0..4095 trên một vòng tròn.
namespace Video {

constexpr int kSamples = 600;               // 600 điểm biên độ cho 360 km
constexpr int kDataBytes = 4 + kSamples;    // azimuth + video[600]
constexpr double kAzimuthLsb = 360.0 / 4096.0;

} // namespace Video
