#pragma once

#include "proto/Packets.h"
#include "track/PlotMerge.h"

#include <QPointF>
#include <QVector>

// Một vị trí cũ của quỹ đạo (vết lịch sử).
struct TrackPoint {
    quint32 azm = 0;       // 0,01 độ
    quint32 range = 0;     // mét
};

// Trạng thái của bộ bám MH (track/MhTracker) cho quỹ đạo track_type = 3 và
// chuỗi điểm dấu đang chờ khởi tạo. Giữ số thực riêng vì trường TRACK làm tròn
// đến m/s và 0,01 độ, lọc trên số đã làm tròn thì hướng bay rung theo từng vòng.
struct MhState {
    QPointF posKm;            // vị trí đã lọc, mặt phẳng tâm đài (km, x đông y bắc)
    QPointF velKmS;           // vận tốc đã lọc, km/s
    QPointF predKm;           // tâm cửa sổ dự đoán của vòng kế tiếp
    PlotMerge::Window window; // cửa sổ dự đoán (cũng ghi vào window_azm/range của TRACK)
    qint64 windowMs = 0;      // lúc mở cửa sổ hiện tại
    int hits = 0;             // số điểm dấu đã nhận vào
    int misses = 0;           // số vòng ngoại suy liên tiếp
};

// Một quỹ đạo trong danh sách: bản ghi theo giao thức TRACK cộng phần chỉ dùng
// trên máy (vết, thời điểm cập nhật, đang theo dõi).
struct TrackEntry {
    quint32 f[Track::Count] = {};
    // Vết suốt đời quỹ đạo, cũ nhất trước, không gồm vị trí hiện tại. Bao nhiêu
    // vết được vẽ là việc của lớp hiển thị (tab "Cài đặt", hoặc toàn bộ khi theo dõi).
    QVector<TrackPoint> history;
    qint64 updatedMs = 0;  // theo đồng hồ đơn điệu của TrackStore
    // Time of Day ASTERIX (1/128 s) lúc nhận vị trí hiện tại. Hợp nhất điểm dấu
    // gửi lại quỹ đạo với vị trí cũ, nên phải kèm đúng giờ của vị trí đó.
    quint32 positionTod = 0;
    bool followed = false;
    // Độ cao radar đo (I048/110) nếu P18M có gửi; TRACK không có trường này
    // nhưng SCH-VQ cần khi quỹ đạo chưa có độ cao từ nhận dạng MH.
    double heightM = 0.0;
    MhState mh;

    // Số vết giữ tối đa cho mỗi quỹ đạo (~14 giờ ở 10 giây một vòng quét): đủ
    // "suốt đời quỹ đạo" mà không để một quỹ đạo kẹt mãi làm phình bộ nhớ.
    static constexpr int kMaxHistory = 5000;

    // Đưa vị trí hiện tại vào vết, ngay trước khi ghi vị trí mới.
    void pushHistory()
    {
        history.append(TrackPoint{f[Track::Azm], f[Track::Range]});
        if (history.size() > kMaxHistory)
            history.remove(0, int(history.size()) - kMaxHistory);
    }

    quint32 id() const { return f[Track::TrackId]; }
    quint32 type() const { return f[Track::TrackType]; }
    double azimuthDeg() const { return f[Track::Azm] / 100.0; }
    double rangeM() const { return double(f[Track::Range]); }
};
