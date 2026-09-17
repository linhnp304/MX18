#pragma once

#include "core/GeoCalc.h"
#include "map/MapPalette.h"

#include <QPixmap>
#include <QPointF>
#include <QWidget>

class MapData;
class QSlider;
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
