#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <optional>

// Gói nhị phân "Cf" của luồng SCN qua UDP: máy "PC" gửi đến MX18 trên dòng
// X18-SCN-R (cổng 10597), MX18 gửi điểm dấu MH cho PC trên dòng X18-SCN-S
// (192.168.232.1:10613). Phân tích ở analysis-results/01-x18-scn.md mục 7.
//
// Khung: Header (2 byte, luôn 04 03) · Length (u16, tổng số byte cả gói) ·
// Type (u16) · các chunk [mã u8][độ dài u8][dữ liệu]. Mọi số little-endian cố
// định — không theo khoá big_endian của connect.json.
namespace ScnCf {

enum MessageType : quint16 {
    MsgNone = 0, MsgWakeUp, MsgShutdown, MsgStatus, MsgInfo, MsgCommand, MsgQuery,
    MsgReply, MsgMark, MsgLine, MsgPosition, MsgTime, MsgPlot, MsgTrack, MsgJammer,
    MsgVideo, MsgKeepAlive, MsgJamLevel, MsgSector, MsgJamStrobe, MsgTypeCount
};

// Mã chunk theo thứ tự enum CfChunkType của SW1 (phụ lục A của file 01); chỉ
// liệt kê những chunk được giải mã.
enum Chunk : quint8 {
    ChNew = 0x04, ChUpdate = 0x05, ChSet = 0x06, ChDelete = 0x07,
    ChTrackId = 0x0B, ChNumber = 0x0C,
    ChPositionXy = 0x1D, ChPositionRAlpha = 0x1E, ChPositionAlpha = 0x20,
    ChVelocityXy = 0x34, ChTrackQuality = 0x3F, ChTargetSource = 0x46,
    ChNorth = 0x4B, ChPsrChannels = 0x4D, ChTransmitMode = 0x4E,
    ChTimeStampWide = 0x50, ChIffNrz = 0x51, ChIffNrzDevice = 0x56, ChIffNrzMode = 0x57,
};

constexpr quint8 kHeader0 = 0x04;
constexpr quint8 kHeader1 = 0x03;
constexpr int kHeadBytes = 6;

// Đơn vị góc của chunk vị trí: milli-arcsec, Bắc = 0, thuận chiều kim đồng hồ.
constexpr double kMasPerDegree = 3600000.0;

struct TimeStamp {
    quint64 sec = 0;               // giờ Unix UTC
    quint32 nsec = 0;
};

struct Message {
    quint16 type = MsgNone;
    quint16 length = 0;            // trường Length trong gói (SW1 không dùng)

    bool isNew = false, isUpdate = false, isSet = false, isDelete = false, north = false;
    std::optional<quint32> trackId;
    std::optional<quint16> number;
    std::optional<qint32> x, y;            // mét, x Đông+, y Bắc+ (có dấu — SW1 đọc sai)
    std::optional<quint32> rangeM;         // PositionRAlpha
    std::optional<quint32> alphaMas;
    std::optional<qint32> positionAlphaMas;
    std::optional<qint16> vx, vy;          // m/s (có dấu — SW1 đọc sai)
    std::optional<quint8> trackQuality;
    std::optional<quint16> targetSource;
    std::optional<quint8> psrChannels;
    QByteArray transmitMode;               // 3 byte thô, rỗng nếu không có
    std::optional<TimeStamp> time;
    std::optional<quint16> iffNrz;
    std::optional<quint8> nrzDevice;       // 1 = KREMNIJ, 2 = PAROL
    std::optional<quint8> nrzMode;         // 1 = MODE_1, 2 = MODE_1_CHECK, 3 = MODE_2, 4 = MODE_3

    int unknownChunks = 0;                 // chunk có mã chưa giải mã (bỏ qua)
};

// Giải mã một datagram. false khi gói hỏng (thiếu đầu gói, chunk vượt cuối
// gói); *error nhận lý do. Chunk lạ không phải lỗi, chỉ đếm vào unknownChunks.
bool decode(const QByteArray &raw, Message *out, QString *error = nullptr);

// Điểm dấu MX18 gửi PC (Type = 12): TimeStampWide · PositionRAlpha ·
// TargetSource · IffNrz, đúng thứ tự và cỡ 38 byte của mẫu SW1.
struct PlotOut {
    TimeStamp time;
    quint32 rangeM = 0;
    quint32 alphaMas = 0;
    quint16 targetSource = 4;      // SW1 gửi 4; gói PC gửi đến mang 1
    quint16 iffNrz = 0x0039;       // giữ như SW1 cho tới khi gói bắt thật cho biết ý nghĩa
};
QByteArray encodePlot(const PlotOut &plot);

// Byte "0" SW1 gửi cho PC lúc mở cổng UDP; giữ lại vì PC có thể dựa vào nó.
QByteArray helloBytes();

QString typeName(quint16 type);
// Một dòng mô tả gói để ghi nhật ký / thông báo.
QString describe(const Message &m);

} // namespace ScnCf

Q_DECLARE_METATYPE(ScnCf::Message)
