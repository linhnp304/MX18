#pragma once

#include <QByteArray>
#include <QtGlobal>

// Khung gói tin dùng chung cho mọi loại dữ liệu trao đổi với hệ thống MH.
//
// Bố cục cố định: header, category, length, serial, time, data_fields[], checksum.
// Mỗi trường 4 byte, nên phần cố định là 24 byte và length = 24 + data.size().
//
// Thứ tự byte không có trong đặc tả; anh Linh chốt mặc định big-endian và cho
// đổi bằng khoá "big_endian" trong ./settings/connect.json, vì vậy mọi hàm đóng
// / mở gói đều nhận cờ này chứ không đọc thẳng cấu hình.
namespace Proto {

constexpr quint32 kHeader = 0x2d2d2d2du;
constexpr int kFixedBytes = 24;          // 5 trường đầu + checksum

enum Category : quint32 {
    CatCmdAt       = 0x9006,   // lệnh điều khiển ăng ten gửi đi
    CatCmdAtBack   = 0x90060,  // trạng thái phản hồi lệnh ăng ten
    CatCmdUser     = 0x9001,   // lệnh điều khiển mức người dùng gửi đi
    CatCmdUserBack = 0x90010,  // trạng thái phản hồi lệnh người dùng
    CatStatusMh    = 0x99910,  // trạng thái hệ thống MH
    CatVideoR      = 0x2011,   // đường quét RD
    CatVideoI      = 0x2012,   // đường quét MH + biên độ

    // Mức kỹ sư: lệnh đi theo phân loại "Cmd-Admin", phản hồi về "Data-Status".
    CatCmdAdmin          = 0x9002,
    CatCmdAdminBack      = 0x90020,
    CatCmdAdminAd        = 0x9003,
    CatCmdAdminAdBack    = 0x90030,
    CatCmdAdminSw        = 0x9004,
    CatCmdAdminSwBack    = 0x90040,
    CatCmdAdminOther     = 0x9005,
    CatCmdAdminOtherBack = 0x90050,
    CatCmdAdminCalibReg  = 0x9007,
    CatCmdAdminCalibRegBack = 0x90070,
    CatCmdAdminBuphabd   = 0x9008,
    CatCmdAdminBuphabdBack = 0x90080,

    CatStatusCalib  = 0x99920, // kết quả hiệu chuẩn, 3..5 gói/giây
    CatStatusParams = 0x99930, // 100 tham số lưu trên hệ thống XL MH
};

struct Frame {
    quint32 category = 0;
    quint32 serial = 0;
    quint32 time = 0;          // ms of day
    QByteArray data;           // data_fields[]
};

// ms kể từ 00:00:00 hôm nay, điền vào trường time.
quint32 msOfDay();

QByteArray build(const Frame &frame, bool bigEndian);

// Trả về false nếu sai header, sai length hoặc gói cụt. Checksum hiện gán 0 nên
// không kiểm tra; khi hệ thống MH bật crc32 thì bổ sung tại đây.
bool parse(const QByteArray &raw, Frame *out, bool bigEndian);

// Đọc trường length của một gói ở đầu buffer (dùng cho luồng TCP, nơi gói tin
// không tự phân định ranh giới). Trả về 0 nếu chưa đủ byte hoặc sai header.
int frameLength(const QByteArray &buffer, bool bigEndian);

// Đóng/mở mảng trường 4 byte — dùng chung cho mọi gói dữ liệu.
QByteArray packFields(const quint32 *fields, int count, bool bigEndian);
bool unpackFields(const QByteArray &data, quint32 *fields, int count, bool bigEndian);

quint32 readU32(const char *p, bool bigEndian);
void writeU32(char *p, quint32 v, bool bigEndian);

} // namespace Proto
