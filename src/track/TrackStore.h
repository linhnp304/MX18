#pragma once

#include "core/GeoCalc.h"
#include "track/MhTracker.h"
#include "track/TrackEntry.h"

#include <QObject>
#include <QVector>

class QTimer;

namespace Asterix {
struct Cat048;
}

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
        RemovedListCleared,    // nút "Xóa danh sách quỹ đạo": như người dùng xoá từng quỹ đạo
        RemovedTrackerOff,     // bỏ chọn "Khởi tạo quỹ đạo từ điểm dấu MH": xoá quỹ đạo track_type 3
        RemovedReplay,         // tua / dừng phát lại: xoá sạch như dừng kết nối nhưng vẫn gửi
                               // bản tin cuối nếu đang gửi thông tin phát lại đến SCH-VQ
    };

    explicit TrackStore(QObject *parent = nullptr);

    void setCenter(double lat, double lon);
    void setDropSeconds(int seconds) { m_dropMs = qint64(seconds) * 1000; }

    // Bản ghi CAT048 từ X18-VQ. Bản ghi không có Track Number là điểm dấu radar
    // nên bị bỏ (như SW0); TRE = 1 thì xoá ngay quỹ đạo đó cùng vết.
    void applyVq(const Asterix::Cat048 &report);

    // Hợp nhất điểm dấu MH (đủ Plot::Count trường) vào quỹ đạo X18-VQ nằm trong
    // cửa sổ ± halfAzDeg / ± halfRangeKm quanh điểm dấu (track/PlotMerge). Trả
    // về true và id quỹ đạo nếu có quỹ đạo nhận; chỉ báo trackUpdated (gửi
    // SCH-VQ) khi nhận dạng hoặc loại quỹ đạo thật sự đổi.
    bool mergePlot(const quint32 *plot, double halfAzDeg, double halfRangeKm, quint32 *mergedId = nullptr);

    // Bộ bám MH (phương án 2, track/MhTracker). Điểm dấu chỉ đưa vào khi "Khởi
    // tạo quỹ đạo từ điểm dấu MH" đang chọn; đường quét VIDEO_I thì luôn đưa vào
    // để chu kỳ quét đo sẵn từ trước.
    void setMhParams(const MhTracker::Params &params) { m_mh.setParams(params); }
    void applyMhPlot(const quint32 *plot);
    void mhSweep(double azDeg);
    // Xoá mọi quỹ đạo track_type 3 cùng các chuỗi chờ khởi tạo, trả về số quỹ đạo.
    int removeMhTracks(RemoveReason reason);
    double mhScanPeriodS() const { return m_mh.scanPeriodS(); }

    bool remove(quint32 id, RemoveReason reason);
    void removeAll(RemoveReason reason);
    bool setFollowed(quint32 id, bool followed);
    // "Xóa nhận dạng": chỉ quỹ đạo track_type = 2, quay về 1 và xoá trường iff_*.
    bool clearIdentity(quint32 id);

    const QVector<TrackEntry> &tracks() const { return m_tracks; }
    const TrackEntry *find(quint32 id) const;

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
    void emitMhEvents(const MhTracker::Events &ev);

    QVector<TrackEntry> m_tracks;
    LocalProjection m_proj;
    QTimer *m_expireTimer = nullptr;
    qint64 m_dropMs = 40000;
    MhTracker m_mh;
};
