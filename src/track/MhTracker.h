#pragma once

#include "track/TrackEntry.h"

#include <QVector>

// Khởi tạo và bám quỹ đạo từ điểm dấu MH (step-06 mục 9, phương án 2). Lớp
// thuần, không QObject, thời gian truyền vào từng lời gọi: TrackStore giữ danh
// sách và phát tín hiệu, còn mx18prototest chạy cả thuật toán với đồng hồ giả.
//
// Nhịp của bộ bám là đường quét MH (azimuth VIDEO_I):
// - Điểm dấu đến thì vào ngay quỹ đạo có cửa sổ dự đoán chứa nó; không có thì
//   nối vào chuỗi chờ khởi tạo; đủ số vòng liên tiếp thì thành quỹ đạo.
// - Đường quét vượt qua cửa sổ của một quỹ đạo mà vòng này chưa có điểm dấu
//   nào rơi vào thì ngoại suy; ngoại suy đủ số vòng mà vẫn không có thì xoá.
//
// Anten quay thuận chiều kim đồng hồ (azimuth tăng dần) như VIDEO_I của MH.
class MhTracker
{
public:
    struct Params {
        int initScans = 2;
        double speedMinMps = 10.0;
        double speedMaxMps = 333.3;
        int extrapolateScans = 3;
        double scanPeriodS = 10.0;     // dùng tới khi đo được chu kỳ quét thật
        double windowAzimuthDeg = 3.0; // nửa bề rộng cửa sổ dự đoán
        double windowRangeKm = 3.0;
        int trackIdStart = 3001;
    };

    struct Events {
        QVector<quint32> added;     // quỹ đạo mới (đã nằm cuối danh sách)
        QVector<quint32> updated;   // có điểm dấu mới hoặc vừa ngoại suy
        QVector<quint32> expired;   // ngoại suy đủ vòng: lớp gọi xoá khỏi danh sách
    };

    // track_id của quỹ đạo MH = kIdBase + số hiệu; track_top = số hiệu. Nhờ vậy
    // không bao giờ đè lên Track Number 12 bit của P18M trong danh sách, còn
    // SCH-VQ chỉ gửi 12 bit thấp nên VQ vẫn thấy đúng số hiệu 3001...
    static constexpr quint32 kIdBase = 0x10000;
    // Điểm dấu đến sau khi chùm tia đã qua mục tiêu; cửa sổ chỉ đóng khi đường
    // quét đã vượt mép cuối thêm ngần này (~0,3 s ở 6 vòng/phút).
    static constexpr double kCloseMarginDeg = 10.0;
    // Bộ lọc alpha-beta: điểm dấu MH đo theo toạ độ cực nên ở 300 km sai số
    // phương vị 0,3° đã là 1,5 km; lấy thẳng vị trí đo thì hướng bay nhảy lung
    // tung. Hệ số kiểu Benedict-Bordner, vẫn đủ nhanh để theo mục tiêu đổi hướng.
    static constexpr double kAlpha = 0.5;
    static constexpr double kBeta = 0.2;

    MhTracker();

    void setParams(const Params &params);
    const Params &params() const { return m_params; }

    // Gọi với mọi tia VIDEO_I. Đo chu kỳ quét mỗi lần qua hướng Bắc và đóng
    // các cửa sổ dự đoán mà đường quét vừa vượt qua.
    void sweep(QVector<TrackEntry> &tracks, double azDeg, qint64 nowMs, Events *ev);

    // Điểm dấu MH (đủ Plot::Count trường). Trả về true nếu điểm dấu được dùng
    // (vào một quỹ đạo hoặc một chuỗi chờ); chế độ phản hồi ngoài 1..9 và điểm
    // dấu trùng (sát quỹ đạo vừa nhận điểm dấu ở vòng này) bị bỏ.
    bool plot(QVector<TrackEntry> &tracks, const quint32 *plot, qint64 nowMs, Events *ev);

    // Bỏ các chuỗi chờ khởi tạo (xoá danh sách quỹ đạo, bỏ chọn khởi tạo từ MH).
    void clearPending() { m_pending.clear(); }
    // Dừng kết nối: quên cả mốc đo chu kỳ quét.
    void reset();

    double scanPeriodS() const { return m_periodS; }
    int pendingCount() const { return int(m_pending.size()); }

    // Thời gian từ lúc chùm tia qua phương vị azFromDeg đến lúc qua azToDeg ở
    // vòng kế tiếp: mục tiêu đi xuôi chiều quay thì anten phải quay quá một vòng.
    static double scanDt(double periodS, double azFromDeg, double azToDeg);

private:
    bool gateOk(const TrackEntry &t, const QPointF &zKm, double azDeg, double rangeM, qint64 nowMs) const;
    void absorb(TrackEntry *t, const QPointF &zKm, double azDeg, const quint32 *plot, qint64 nowMs);
    void openWindow(TrackEntry *t, qint64 nowMs) const;
    void writeFields(TrackEntry *t) const;
    void extrapolate(TrackEntry *t, qint64 nowMs);
    void purgePending(qint64 nowMs);
    quint32 allocateNumber(const QVector<TrackEntry> &tracks);

    Params m_params;
    // Chuỗi điểm dấu chờ khởi tạo, mỗi chuỗi là một TrackEntry chưa vào danh
    // sách: nhận dạng và vết tích luỹ sẵn để quỹ đạo sinh ra có ngay đủ cả hai.
    QVector<TrackEntry> m_pending;
    double m_periodS = 10.0;
    bool m_periodMeasured = false;
    bool m_hasSweep = false;
    double m_prevAzDeg = 0.0;
    qint64 m_northMs = -1;
    int m_nextNumber = 3001;
};
