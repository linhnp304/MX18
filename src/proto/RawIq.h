#pragma once

#include <QString>
#include <QtGlobal>

// Gói RAW_IQ nhận qua dòng "Data-RAW" (docs/step-04.md).
//
// Khác mọi gói khác, RAW_IQ không có khung Dataframe: cả datagram là mảng
// RawIQ[4805] kiểu unsigned int. Thứ tự byte dùng chung khoá big_endian của
// connect.json (anh Linh chốt 2026-09-24). Mỗi word dữ liệu ghép hai số Int16:
// 16 bit thấp là IQ1 / Sum, 16 bit cao là IQ2 / Sub.
namespace RawIq {

constexpr int kWords = 4805;
constexpr int kBytes = kWords * 4;          // 19220 byte — cũng là cỡ cắt dòng TCP
constexpr int kDataOffset = 3;              // dữ liệu nằm ở RawIQ[3..4802]
constexpr int kDataWords = 4800;
// Hai word đuôi chưa dùng đến, nên gói thiếu chúng vẫn tính được.
constexpr int kMinBytes = (kDataOffset + kDataWords) * 4;

constexpr int kAzimuthSteps = 4096;

// ViewIQ: 600 điểm liên tiếp bắt đầu từ StartWord.
constexpr int kViewPoints = 600;
constexpr int kViewStartMax = 4199;
// Vẽ CS: trung bình cộng MeanWords word bắt đầu từ StartWord.
constexpr int kMeanMin = 50;
constexpr int kMeanMax = 500;
constexpr int kMeanDefault = 100;
constexpr int kStartDefault = 2000;
inline int beamStartMax(int meanWords) { return 4799 - meanWords; }

// Hai giá trị nhiễu hệ thống MH chèn vào dòng dữ liệu, không được dùng.
constexpr qint16 kNoiseA = 0x5a5a;           // 23130
constexpr qint16 kNoiseB = -23131;           // 0xa5a5
inline bool isNoise(qint16 v) { return v == kNoiseA || v == kNoiseB; }

enum Type { CsF2 = 2, CsF3 = 3, F2I = 4, F2Q = 5, F3I = 6, F3Q = 7 };
inline bool isValidType(int t) { return t >= CsF2 && t <= F3Q; }
inline bool isBeamType(int t) { return t == CsF2 || t == CsF3; }
// "CS F2", "F2-I"…; rỗng nếu ngoài dải.
QString typeName(int t);

inline double azimuthDeg(int azm4096) { return azm4096 * 360.0 / kAzimuthSteps; }

struct Header {
    int type = 0;
    int azm4096 = 0;
};

// false khi gói cụt hoặc IQType ngoài 2..7 — đặc tả bảo bỏ qua cả gói.
bool readHeader(const char *raw, int size, bool bigEndian, Header *out);

// ViewIQ: tách 600 cặp IQ từ StartWord (tự kẹp về 0..4199). Giá trị nhiễu
// thay bằng điểm liền trước của cùng kênh, điểm đầu tiên thì bằng 0.
// raw phải có ít nhất kMinBytes byte.
void extractView(const char *raw, bool bigEndian, int startWord, qint16 *iq1, qint16 *iq2);

// Vẽ CS: trung bình cộng Sum / Sub, bỏ các giá trị nhiễu ra khỏi phép tính.
// Kênh nào toàn nhiễu thì trả NaN (không có gì để lấy trung bình).
void beamMeans(const char *raw, bool bigEndian, int startWord, int meanWords,
               double *sumMean, double *subMean);

// ViewType "dB" = 20·log10(mean). Trung bình dưới 1 kẹp về 0 dB (anh Linh chốt
// 2026-09-24) để đường cánh sóng chạm sàn chứ không đứt; NaN giữ nguyên.
double toDb(double mean);

} // namespace RawIq
