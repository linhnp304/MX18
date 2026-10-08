#pragma once

#include <QPointF>
#include <QVector>

struct TrackEntry;

// Hợp nhất điểm dấu MH vào quỹ đạo X18-VQ (step-06 mục 8, phương án 1). Tách
// khỏi TrackStore để kiểm thử thuật toán mà không cần dựng danh sách.
//
// Cửa sổ chỉ tồn tại lúc xét một điểm dấu: không có quỹ đạo nào rơi vào thì bỏ
// qua vòng quét này (anh Linh chốt 2026-10-08), không giữ cửa sổ chờ quỹ đạo
// cập nhật sau.
namespace PlotMerge {

// Cự ly tối đa của màn hình: cửa sổ không vượt quá vòng 360 km.
constexpr double kMaxRangeM = 360000.0;

struct Window {
    double azStartDeg = 0.0;   // azStart > azStop: cửa sổ vắt qua hướng Bắc
    double azStopDeg = 0.0;
    double rangeStartM = 0.0;
    double rangeStopM = 0.0;
    bool fullCircle = false;   // nửa cỡ phương vị ≥ 180°: mọi phương vị đều lọt

    bool contains(double azDeg, double rangeM) const;
};

// Cửa sổ quanh điểm dấu: ± halfAzDeg theo phương vị, ± halfRangeM theo cự ly
// ("kích thước cửa sổ hợp nhất" của tab "Thiết lập khác" là nửa bề rộng).
Window window(double azDeg, double rangeM, double halfAzDeg, double halfRangeM);

// Chỉ số quỹ đạo nhận điểm dấu, -1 nếu không quỹ đạo nào lọt cửa sổ. Ưu tiên
// quỹ đạo đã hợp nhất ở vòng trước (track_type 2), sau đó quỹ đạo gần điểm dấu
// nhất. Quỹ đạo tự bám từ MH (track_type 3) không tham gia.
int pick(const QVector<TrackEntry> &tracks, const Window &w, const QPointF &plotPlane);

// Ghi nhận dạng của điểm dấu (đủ Plot::Count trường) vào quỹ đạo. Chế độ phản
// hồi luôn lấy lần mới nhất; tốp chỉ huy / số hiệu / độ cao / nhiên liệu chỉ ghi
// đè khi điểm dấu đúng chế độ và có giá trị khác 0, nên thông tin đã có được giữ
// suốt đời quỹ đạo. Trả về true nếu quỹ đạo có gì thay đổi.
bool applyIdentity(TrackEntry *t, const quint32 *plot);

// Chế độ phản hồi hợp lệ để hợp nhất (1..9).
bool validMode(quint32 retmode);

} // namespace PlotMerge
