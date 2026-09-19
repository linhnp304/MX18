#pragma once

#include "core/GeoCalc.h"
#include "map/MapPalette.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QVector>
#include <QWidget>

class MapData;
class QSlider;
class QTimer;
class QToolButton;

// Panel 1: nền bản đồ số và mọi đối tượng đồ hoạ vẽ trên đó.
//
// Toàn bộ lớp bản đồ tĩnh được kết xuất sẵn vào một QPixmap và chỉ dựng lại khi
// khung nhìn hoặc tuỳ chọn hiển thị thay đổi; các giai đoạn sau vẽ video, điểm
// dấu và quỹ đạo đè lên trên với tần suất cao mà không phải vẽ lại bản đồ.
class MapView : public QWidget
{
    Q_OBJECT
public:
    explicit MapView(QWidget *parent = nullptr);

    void setMapData(MapData *data);
    const LocalProjection &projection() const { return m_proj; }

    // Đổi tâm đài: chiếu lại toàn bộ hình học rồi vẽ lại.
    void setRadarCenter(double lat, double lon);

    // Đưa tâm đài về chính giữa panel và zoom vừa vòng cự ly tối đa.
    void centerOnRadar();

    // Gọi khi tuỳ chọn trong tab "Cài đặt" hoặc bảng màu thay đổi.
    void refreshSettings();

    // Đường quét RD (gói VIDEO_R) và đường quét MH kèm 600 điểm biên độ
    // (gói VIDEO_I). Cả hai đến khoảng 400 lần/giây nên chỉ ghi lại dữ liệu;
    // việc vẽ do bộ đếm thời gian 25 hình/giây bên trong lo.
    void setRadarSweep(double azimuthDeg);
    void setMhSweep(double azimuthDeg, const QByteArray &video);
    void clearVideo();

signals:
    // valid = false khi con trỏ rời khỏi panel.
    void cursorGeoChanged(bool valid, double lat, double lon, double bearing, double range);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QTransform viewTransform() const;
    double fitScale() const;
    void setScale(double scale, const QPointF &anchorScreen);
    void invalidateCache();
    void rebuildCache();
    void drawLayers(QPainter &p, const QRectF &viewPlane, int lod);
    void drawGrid(QPainter &p, const QRectF &viewPlane);
    void drawPoints(QPainter &p, const QRectF &viewPlane);
    void drawInfoBox(QPainter &p);
    void drawSweepLines(QPainter &p);
    void ensureVideoLayer();
    void fadeVideoLayer(double seconds);
    void flushVideo();
    void layoutZoomBar();
    void syncZoomSlider();
    void onZoomSlider(int value);

    MapData *m_data = nullptr;
    LocalProjection m_proj;
    MapPalette m_palette;

    QPointF m_viewCenter;      // toạ độ mặt phẳng (km) ở giữa panel
    double m_scale = 1.0;      // pixel trên km

    QPixmap m_cache;
    bool m_cacheDirty = true;

    // Biên độ tích luỹ vào một lớp ARGB riêng rồi mờ dần theo thời gian, nhờ vậy
    // vệt quét cũ còn lại trên màn hình mà không phải vẽ lại hàng nghìn tia mỗi
    // khung hình.
    struct Spoke { double az; QByteArray video; };
    QVector<Spoke> m_pendingSpokes;
    QImage m_videoLayer;
    QTimer *m_videoTimer = nullptr;
    QElapsedTimer m_fadeClock;
    double m_fadeCarry = 0.0;      // phần lẻ của mức alpha phải trừ, dồn sang lần sau

    double m_sweepRd = 0.0;
    double m_sweepMh = 0.0;
    bool m_hasSweepRd = false;
    bool m_hasSweepMh = false;

    // Khung nhìn còn bám theo kích thước panel cho tới khi trắc thủ tự zoom/kéo.
    bool m_fitPending = true;

    bool m_panning = false;
    QPointF m_panLastScreen;

    QWidget *m_zoomBar = nullptr;
    QSlider *m_zoomSlider = nullptr;
    QToolButton *m_zoomOut = nullptr;
    QToolButton *m_zoomIn = nullptr;
    bool m_updatingSlider = false;

    QPixmap m_logo;
};
