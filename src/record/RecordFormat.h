#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

#include <array>

class QIODevice;

// Định dạng file ghi lưu ./records/yyyy/MM/yyyyMMdd_HHmmss.rec (step-07, thiết
// kế ở analysis-results/07 mục 2.2). Mọi số trong file là little-endian.
//
//   header   64 × u32 = 256 byte, ghi lại định kỳ trong lúc ghi
//   khối     16 byte đầu khối + các bản ghi gói, mỗi nhịp ghi một khối
//   bản ghi  u32 ms từ đầu file, u16 độ dài, u8 dòng, u8 cờ, rồi byte của gói
//
// Không có chỉ mục ở cuối file: phát lại dựng bảng thời gian → vị trí bằng cách
// đi dọc đầu khối, nhờ vậy file ghi dở (mất điện, phần mềm bị giết) vẫn phát
// lại và tua được tới khối cuối cùng còn nguyên.
namespace RecordFormat {

constexpr quint32 kMagic = 0xcafe6869u;
constexpr quint32 kBlockSync = 0xb10c6869u;
// Header[63] mang dấu này khi file được đóng đàng hoàng; thiếu dấu thì số liệu
// trong header có thể cũ hơn dữ liệu tới một nhịp cập nhật header.
constexpr quint32 kClosedMark = 0x434c4f53u; // "CLOS"
constexpr quint32 kVersion = 1;

constexpr int kHeaderFields = 64;
constexpr int kHeaderBytes = kHeaderFields * 4;
constexpr int kBlockHeadBytes = 16;
constexpr int kPacketHeadBytes = 8;
constexpr int kMaxPacketBytes = 0xffff;
// Gói VIDEO_R / VIDEO_I rút gọn: 5 trường đầu gói + azimuth.
constexpr int kShortVideoBytes = 24;

// Bản ghi mang số dòng này là khối mô tả (JSON) ở đầu file, không phải gói nhận.
constexpr quint8 kMetaStream = 0xff;

// Cờ của từng bản ghi.
enum PacketFlag : quint8 {
    PacketTruncated = 0x01,    // gói bị cắt còn kShortVideoBytes
};

// Phân loại để đếm trong header, vị trí cố định theo thứ tự step-07 nên đọc
// header không cần khối mô tả. Data-Status tách hai ô: PLOT và gói khác.
enum Kind {
    KindVideoR = 0,
    KindVideoI,
    KindPlot,
    KindStatusOther,
    KindParams,
    KindRawIq,
    KindScn,
    KindScnR,
    KindVq,
    KindOther,
    KindCount
};

enum Field {
    FMagic = 0,
    FStartSec,                 // giờ bắt đầu ghi, Unix giây (UTC)
    FEndSec,                   // giờ kết thúc ghi
    FFileSize,                 // byte
    FTotal,                    // tổng số gói nhận (không tính khối mô tả)
    FCountBase,                // FCountBase + Kind: số gói từng loại
    // Ô dự phòng còn lại để trống; vài ô cuối dùng cho chính định dạng.
    FVersion = 56,
    FStartMs,                  // phần ms của giờ bắt đầu / kết thúc
    FEndMs,
    FFlags,                    // FileFlag: lựa chọn ghi của records.json lúc ghi
    FBlocks,                   // số khối, kể cả khối mô tả
    FDurationMs,               // ms từ đầu file tới gói cuối cùng
    FReserved62,
    FClosed = 63,
};

enum FileFlag : quint32 {
    FileFullVideoR = 0x01,
    FileFullVideoI = 0x02,
    FileRawIq      = 0x04,
};

struct Header {
    std::array<quint32, kHeaderFields> f{};

    QByteArray encode() const;
    // false khi thiếu byte hoặc sai định danh 0xcafe6869.
    bool decode(const char *data, int size);

    bool closed() const { return f[FClosed] == kClosedMark; }
    QDateTime startTime() const;
    QDateTime endTime() const;
    qint64 recordedMs() const;           // giờ kết thúc - giờ bắt đầu
    quint32 count(int kind) const { return f[FCountBase + kind]; }
};

// Phân loại một dòng connect.json theo tên (đúng tên trong bảng mặc định của
// LinkConfig) và định dạng; dòng lạ là KindOther.
int kindForStream(const QString &category, int format);
// Loại của một gói cụ thể: Data-Status (KindStatusOther) tách riêng PLOT theo
// category ở byte 4..7, đọc theo thứ tự byte của dòng.
int kindForPacket(int streamKind, const char *data, int size, bool bigEndian);
QString kindName(int kind);

struct BlockHead {
    quint32 sync = kBlockSync;
    quint32 bytes = 0;         // cả khối, kể cả 16 byte đầu
    quint32 firstMs = 0;       // ms từ đầu file của gói đầu khối
    quint32 packets = 0;
};

void writeBlockHead(char *out, const BlockHead &head);
bool readBlockHead(const char *in, BlockHead *head);

// Nối một bản ghi gói vào cuối buffer.
void appendPacket(QByteArray *out, quint32 ms, quint8 stream, quint8 flags,
                  const char *data, int size);

struct Packet {
    quint32 ms = 0;
    quint8 stream = 0;
    quint8 flags = 0;
    const char *data = nullptr;
    int size = 0;
};

// Đọc bản ghi gói tại *offset trong một khối (đã bỏ 16 byte đầu khối), tiến
// *offset qua nó. false khi hết khối hoặc bản ghi tràn ra ngoài khối.
bool nextPacket(const QByteArray &block, int *offset, Packet *out);

struct BlockRef {
    qint64 offset = 0;         // vị trí đầu khối trong file
    quint32 bytes = 0;
    quint32 firstMs = 0;
    quint32 packets = 0;
};

// Đi dọc các khối từ sau header tới khi hết file hoặc gặp khối hỏng (file ghi
// dở). *validBytes nhận vị trí cuối khối nguyên vẹn cuối cùng. Nếu recount
// khác null thì đọc cả dữ liệu để đếm lại số gói từng loại vào đó (cần bảng
// dòng → loại trong khối mô tả, xem readMeta) và FDurationMs.
bool walkBlocks(QIODevice *file, QVector<BlockRef> *blocks, qint64 *validBytes,
                Header *recount = nullptr, QString *error = nullptr);

// Khối mô tả: JSON trong bản ghi kMetaStream của khối đầu tiên.
QByteArray readMeta(QIODevice *file, QString *error = nullptr);

} // namespace RecordFormat
