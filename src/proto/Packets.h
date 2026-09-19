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

// Đường quét: azimuth 0..4095 trên một vòng tròn.
namespace Video {

constexpr int kSamples = 600;               // 600 điểm biên độ cho 360 km
constexpr int kDataBytes = 4 + kSamples;    // azimuth + video[600]
constexpr double kAzimuthLsb = 360.0 / 4096.0;

} // namespace Video
