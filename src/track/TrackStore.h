#pragma once

#include "core/GeoCalc.h"
#include "proto/Packets.h"

#include <QElapsedTimer>
#include <QObject>
#include <QVector>

class QTimer;

namespace Asterix {
struct Cat048;
}

// Một vị trí cũ của quỹ đạo (vết lịch sử).
struct TrackPoint {
    quint32 azm = 0;       // 0,01 độ
    quint32 range = 0;     // mét
};

// Một quỹ đạo trong danh sách: bản ghi theo giao thức TRACK cộng phần chỉ dùng
// trên máy (vết, thời điểm cập nhật, đang theo dõi).
struct TrackEntry {
    quint32 f[Track::Count] = {};
    // Vết suốt đời quỹ đạo, cũ nhất trước, không gồm vị trí hiện tại. Bao nhiêu
    // vết được vẽ là việc của lớp hiển thị (tab "Cài đặt", hoặc toàn bộ khi theo dõi).
    QVector<TrackPoint> history;
    qint64 updatedMs = 0;  // theo đồng hồ đơn điệu của TrackStore
    bool followed = false;
    // Độ cao radar đo (I048/110) nếu P18M có gửi; TRACK không có trường này
    // nhưng SCH-VQ cần khi quỹ đạo chưa có độ cao từ nhận dạng MH.
    double heightM = 0.0;

    quint32 id() const { return f[Track::TrackId]; }
    quint32 type() const { return f[Track::TrackType]; }
    double azimuthDeg() const { return f[Track::Azm] / 100.0; }
    double rangeM() const { return double(f[Track::Range]); }
};

// Danh sách quỹ đạo, khoá theo track_id, giữ đúng thứ tự xuất hiện (tab "Danh
// sách" thêm dòng mới ở cuối). Sống trên luồng giao diện: X18-VQ chỉ vài chục
// bản ghi mỗi vòng quét nên không cần chỗ hẹn đa luồng như RAW_IQ.
class TrackStore : public QObject
{
    Q_OBJECT
public:
    enum RemoveReason {
        RemovedTimeout = 0,    // quá "Thời gian xóa quỹ đạo khi không có cập nhật"
        RemovedEndOfTrack,     // P18M gửi bản tin cuối (I048/170 TRE = 1)
        RemovedByUser,         // menu "Xóa", nút xoá của tab "Danh sách"
        RemovedExtrapolated,   // bộ bám MH ngoại suy đủ số vòng
        RemovedDisconnected,   // dừng kết nối: xoá sạch, không báo đi đâu
    };

    explicit TrackStore(QObject *parent = nullptr);

    void setCenter(double lat, double lon);
    void setDropSeconds(int seconds) { m_dropMs = qint64(seconds) * 1000; }

    // Bản ghi CAT048 từ X18-VQ. Bản ghi không có Track Number là điểm dấu radar
    // nên bị bỏ (như SW0); TRE = 1 thì xoá ngay quỹ đạo đó cùng vết.
    void applyVq(const Asterix::Cat048 &report);

    bool remove(quint32 id, RemoveReason reason);
    void removeAll(RemoveReason reason);
    bool setFollowed(quint32 id, bool followed);
    // "Xóa nhận dạng": chỉ quỹ đạo track_type = 2, quay về 1 và xoá trường iff_*.
    bool clearIdentity(quint32 id);

    const QVector<TrackEntry> &tracks() const { return m_tracks; }
    const TrackEntry *find(quint32 id) const;

    // Số vết giữ tối đa cho mỗi quỹ đạo (~14 giờ ở 10 giây một vòng quét): đủ
    // "suốt đời quỹ đạo" mà không để một quỹ đạo kẹt mãi làm phình bộ nhớ.
    static constexpr int kMaxHistory = 5000;

signals:
    // Dữ liệu mới từ nguồn (X18-VQ, hợp nhất, bộ bám MH): đây là lúc SCH-VQ gửi đi.
    void trackUpdated(const TrackEntry &track, bool added);
    // Đổi trạng thái trên máy (theo dõi, xoá nhận dạng): chỉ vẽ lại, không gửi.
    void trackChanged(quint32 id);
    void trackRemoved(const TrackEntry &track, int reason);

private:
    void expire();
    int indexOf(quint32 id) const;
    void setLatLng(TrackEntry *t) const;

    QVector<TrackEntry> m_tracks;
    LocalProjection m_proj;
    QElapsedTimer m_clock;
    QTimer *m_expireTimer = nullptr;
    qint64 m_dropMs = 40000;
};
