#pragma once

#include "proto/Packets.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

class QFont;
class QPainter;
class QTransform;
class TrackStore;
struct TrackEntry;

// Chữ của thông tin nhận dạng MH, dùng chung cho ô thông tin điểm dấu, khung
// theo dõi và cửa sổ thông tin quỹ đạo (step-06 mục 6, 7).
namespace IffText {

// "Chế độ 1".."Chế độ 6", "BÁO ĐỘNG", "Báo nạn CĐ 1", "Báo nạn CĐ 2"; bold = true
// với ba chế độ khẩn 7, 8, 9. Chuỗi rỗng khi chưa có chế độ (0) hoặc chế độ lạ.
QString mode(quint32 retmode, bool *bold = nullptr);

// Các dòng chỉ hiện ở đúng chế độ của nó: tốp chỉ huy (3), số hiệu (4), độ
// cao và nhiên liệu (6); giá trị 0 nghĩa là không có. Dùng cho điểm dấu MH.
QStringList details(quint32 retmode, quint32 commander, quint32 flightid,
                    quint32 altitude, quint32 fuel);

// Nhận dạng quỹ đạo đang giữ: hiện mọi trường khác 0 bất kể chế độ mới nhất,
// vì nhận dạng hợp nhất được giữ suốt đời quỹ đạo — vòng chế độ 1 vẫn phải
// thấy số hiệu lấy từ vòng chế độ 4 (anh Linh chốt 2026-10-08).
QStringList held(quint32 commander, quint32 flightid, quint32 altitude, quint32 fuel);

} // namespace IffText

// Lớp mục tiêu vẽ đè lên panel 1: quỹ đạo (đọc thẳng từ TrackStore) kèm vết,
// lý lịch và khung theo dõi; điểm dấu MH; tia báo động. Không phải widget —
// MapView gọi draw() trong paintEvent sau lớp video, nên không đụng tới bộ đệm
// nền bản đồ.
class TargetLayer
{
public:
    TargetLayer();

    void setTrackStore(const TrackStore *store) { m_tracks = store; }

    void addPlot(const quint32 *fields);    // đủ Plot::Count trường
    void clearPlots();
    // Mỗi gói ALARM_HEAD là một tia; gói mới cùng hướng chỉ làm mới thời gian.
    void addAlarm(double headDeg);
    void clearAlarms();

    // Bỏ điểm dấu và tia báo động đã quá "Thời gian hiển thị điểm dấu MH".
    // Trả về true khi vẫn còn thứ phải nhấp nháy hoặc chờ hết hạn.
    bool prune();

    // pxPerKm là tỉ lệ đang vẽ; t đổi mặt phẳng (km) ra toạ độ màn hình; view
    // là khung panel, để chữ và ô thông tin sát mép không bị cắt.
    void draw(QPainter &p, const QTransform &t, double pxPerKm, const QRectF &view,
              const QFont &font) const;

    // Quỹ đạo gần pos nhất (toạ độ màn hình) trong phạm vi ký hiệu.
    bool trackAt(const QTransform &t, const QPointF &pos, quint32 *id) const;

    // Cạnh ô vuông bao ký hiệu quỹ đạo theo "Kích thước quỹ đạo" (pixel).
    static double trackSymbolSize();
    // Toạ độ mặt phẳng (km) của vị trí hiện tại của quỹ đạo.
    static QPointF trackPlane(const TrackEntry &t);

private:
    struct PlotMark {
        quint32 f[Plot::Count];
        QPointF plane;      // km
        qint64 ms;          // theo DataClock
    };
    struct AlarmRay {
        double headDeg;
        qint64 ms;
    };

    void drawAlarms(QPainter &p, const QTransform &t, double pxPerKm, const QRectF &view,
                    const QFont &font) const;
    void drawTracks(QPainter &p, const QTransform &t, const QFont &font) const;
    void drawPlots(QPainter &p, const QTransform &t, const QRectF &view, const QFont &font) const;

    const TrackStore *m_tracks = nullptr;
    QVector<PlotMark> m_plots;
    QVector<AlarmRay> m_alarms;
    // Nhịp nhấp nháy tia báo động theo đồng hồ máy: tạm dừng phát lại vẫn nháy
    // cho thấy đó là báo động.
    QElapsedTimer m_clock;
};
