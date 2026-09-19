#include "ui/MapView.h"

#include "core/AppPaths.h"
#include "core/Settings.h"
#include "map/MapData.h"
#include "proto/Packets.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QFontInfo>
#include <QPainterPath>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

// Giới hạn zoom tuyệt đối (pixel trên km): xa nhất nhìn được cả Đông Nam Á,
// gần nhất đủ để tách hai sân bay cách nhau vài trăm mét.
constexpr double kScaleMin = 0.08;
constexpr double kScaleMax = 140.0;
constexpr int kZoomSteps = 1000;

// Dưới mức này chữ trên bản đồ chồng lên nhau nên chỉ vẽ ký hiệu, bỏ nhãn.
constexpr double kLabelScale = 0.8;

// 25 hình/giây là đủ mượt cho mắt mà vẫn rẻ so với 400 gói video mỗi giây.
constexpr int kVideoFrameMs = 40;

// Chặn hàng đợi tia quét: nếu giao diện kẹt vài giây thì bỏ tia cũ chứ không để
// bộ nhớ phình theo.
constexpr int kMaxPendingSpokes = 1200;

const QColor kSweepRdColor(0x3f, 0xa9, 0xf5);  // xanh biển: đường quét RD
const QColor kSweepMhColor(0x3c, 0xff, 0x6a);  // xanh lá: đường quét MH

// Font khai báo theo pixel trả về pointSizeF() = -1, nên phải hỏi QFontInfo.
double basePointSize(const QFont &f)
{
    return f.pointSizeF() > 0.0 ? f.pointSizeF() : QFontInfo(f).pointSizeF();
}

double scaleFromSlider(int v)
{
    const double t = double(qBound(0, v, kZoomSteps)) / kZoomSteps;
    return kScaleMin * std::pow(kScaleMax / kScaleMin, t);
}

int sliderFromScale(double s)
{
    const double t = std::log(qBound(kScaleMin, s, kScaleMax) / kScaleMin)
                     / std::log(kScaleMax / kScaleMin);
    return int(qRound(t * kZoomSteps));
}

} // namespace

MapView::MapView(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_palette = MapPalette::forBrightness(Settings::instance().setups().brightness);
    m_proj.setCenter(Settings::instance().setups().radarLat,
                     Settings::instance().setups().radarLon);
    m_logo.load(AppPaths::resourceFile(QStringLiteral("logo.png")));

    // Thanh trượt zoom ở góc dưới bên phải panel.
    m_zoomBar = new QWidget(this);
    m_zoomBar->setObjectName(QStringLiteral("ZoomBar"));
    m_zoomBar->setStyleSheet(QStringLiteral(
        "#ZoomBar { background: rgba(28,33,39,215); border: 1px solid #454f5a;"
        " border-radius: 4px; }"
        "#ZoomBar QToolButton { background: #2b323a; color: #cfd8e2; border: 1px solid #454f5a;"
        " border-radius: 3px; font-weight: bold; padding: 0px; }"
        "#ZoomBar QToolButton:hover { background: #38414b; }"));

    m_zoomOut = new QToolButton(m_zoomBar);
    m_zoomOut->setText(QStringLiteral("−"));
    m_zoomOut->setToolTip(QStringLiteral("Thu nhỏ bản đồ"));
    m_zoomOut->setFixedSize(20, 18);
    m_zoomIn = new QToolButton(m_zoomBar);
    m_zoomIn->setText(QStringLiteral("+"));
    m_zoomIn->setToolTip(QStringLiteral("Phóng to bản đồ"));
    m_zoomIn->setFixedSize(20, 18);

    m_zoomSlider = new QSlider(Qt::Horizontal, m_zoomBar);
    m_zoomSlider->setRange(0, kZoomSteps);
    m_zoomSlider->setFixedWidth(130);
    m_zoomSlider->setToolTip(QStringLiteral("Zoom bản đồ"));

    auto *lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(5, 3, 5, 3);
    lay->setSpacing(5);
    lay->addWidget(m_zoomOut);
    lay->addWidget(m_zoomSlider);
    lay->addWidget(m_zoomIn);
    m_zoomBar->adjustSize();

    connect(m_zoomSlider, &QSlider::valueChanged, this, &MapView::onZoomSlider);
    connect(m_zoomOut, &QToolButton::clicked, this, [this] {
        m_zoomSlider->setValue(m_zoomSlider->value() - kZoomSteps / 25);
    });
    connect(m_zoomIn, &QToolButton::clicked, this, [this] {
        m_zoomSlider->setValue(m_zoomSlider->value() + kZoomSteps / 25);
    });

    m_videoTimer = new QTimer(this);
    m_videoTimer->setInterval(kVideoFrameMs);
    connect(m_videoTimer, &QTimer::timeout, this, &MapView::flushVideo);
}

void MapView::setMapData(MapData *data)
{
    m_data = data;
    if (m_data)
        m_data->project(m_proj);
    centerOnRadar();
}

void MapView::setRadarCenter(double lat, double lon)
{
    m_proj.setCenter(lat, lon);
    if (m_data)
        m_data->project(m_proj);
    invalidateCache();
    update();
}

double MapView::fitScale() const
{
    // Vòng cự ly tối đa phải nằm gọn trong panel, chừa 4% lề.
    const double side = qMax(80, qMin(width(), height()));
    return (side * 0.96) / (2.0 * kMaxRangeKm);
}

void MapView::centerOnRadar()
{
    m_viewCenter = QPointF(0.0, 0.0);
    m_scale = qBound(kScaleMin, fitScale(), kScaleMax);
    m_fitPending = true;
    syncZoomSlider();
    invalidateCache();
    update();
}

void MapView::refreshSettings()
{
    m_palette = MapPalette::forBrightness(Settings::instance().setups().brightness);
    invalidateCache();
    update();
}

QTransform MapView::viewTransform() const
{
    QTransform t;
    t.translate(width() / 2.0, height() / 2.0);
    t.scale(m_scale, -m_scale); // trục y của mặt phẳng hướng bắc, của màn hình hướng xuống
    t.translate(-m_viewCenter.x(), -m_viewCenter.y());
    return t;
}

void MapView::setScale(double scale, const QPointF &anchorScreen)
{
    const double s = qBound(kScaleMin, scale, kScaleMax);
    if (qFuzzyCompare(s, m_scale))
        return;

    // Giữ nguyên điểm dưới con trỏ khi phóng to/thu nhỏ.
    bool ok = false;
    const QTransform inv = viewTransform().inverted(&ok);
    const QPointF anchorPlane = ok ? inv.map(anchorScreen) : m_viewCenter;

    m_scale = s;
    m_fitPending = false;
    const QPointF offset = anchorScreen - QPointF(width() / 2.0, height() / 2.0);
    m_viewCenter = anchorPlane - QPointF(offset.x() / m_scale, -offset.y() / m_scale);

    syncZoomSlider();
    invalidateCache();
    update();
}

void MapView::syncZoomSlider()
{
    if (!m_zoomSlider)
        return;
    m_updatingSlider = true;
    m_zoomSlider->setValue(sliderFromScale(m_scale));
    m_updatingSlider = false;
}

void MapView::onZoomSlider(int value)
{
    if (m_updatingSlider)
        return;
    setScale(scaleFromSlider(value), QPointF(width() / 2.0, height() / 2.0));
}

void MapView::invalidateCache()
{
    m_cacheDirty = true;
    // Vệt biên độ tích luỹ theo toạ độ màn hình; đổi khung nhìn thì vệt cũ nằm
    // sai chỗ nên phải xoá chứ không dời được.
    if (!m_videoLayer.isNull())
        m_videoLayer.fill(Qt::transparent);
    m_fadeCarry = 0.0;
}

void MapView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutZoomBar();
    // Lần hiện đầu tiên panel chưa có kích thước thật, nên phải tính lại tỉ lệ
    // vừa vòng 360 km khi panel nhận kích thước cuối cùng.
    if (m_fitPending) {
        m_scale = qBound(kScaleMin, fitScale(), kScaleMax);
        syncZoomSlider();
    }
    invalidateCache();
}

void MapView::layoutZoomBar()
{
    if (!m_zoomBar)
        return;
    const QSize s = m_zoomBar->sizeHint();
    m_zoomBar->setGeometry(width() - s.width() - 12, height() - s.height() - 12,
                           s.width(), s.height());
}

// --------------------------------------------------------------------- chuột

void MapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_panLastScreen = event->position();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();

    if (m_panning) {
        const QPointF d = pos - m_panLastScreen;
        m_panLastScreen = pos;
        m_viewCenter -= QPointF(d.x() / m_scale, -d.y() / m_scale);
        m_fitPending = false;
        invalidateCache();
        update();
    }

    bool ok = false;
    const QTransform inv = viewTransform().inverted(&ok);
    if (ok) {
        const QPointF plane = inv.map(pos);
        const GeoPoint g = m_proj.toGeo(plane);
        const double bearing = std::fmod(std::atan2(plane.x(), plane.y()) * 180.0 / M_PI + 360.0, 360.0);
        emit cursorGeoChanged(true, g.lat, g.lon, bearing, std::hypot(plane.x(), plane.y()));
    }
    QWidget::mouseMoveEvent(event);
}

void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(Qt::CrossCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

void MapView::wheelEvent(QWheelEvent *event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps)) {
        event->ignore();
        return;
    }
    setScale(m_scale * std::pow(1.18, steps), event->position());
    event->accept();
}

void MapView::leaveEvent(QEvent *event)
{
    emit cursorGeoChanged(false, 0, 0, 0, 0);
    QWidget::leaveEvent(event);
}

// ---------------------------------------------------------------------- vẽ

void MapView::paintEvent(QPaintEvent *)
{
    const QSize want(qRound(width() * devicePixelRatioF()), qRound(height() * devicePixelRatioF()));
    if (m_cacheDirty || m_cache.size() != want)
        rebuildCache();

    QPainter p(this);
    p.drawPixmap(0, 0, m_cache);
    if (!m_videoLayer.isNull())
        p.drawImage(QPointF(0, 0), m_videoLayer);
    drawSweepLines(p);
    drawInfoBox(p);
}

// ---------------------------------------------------- đường quét và biên độ

void MapView::setRadarSweep(double azimuthDeg)
{
    m_sweepRd = azimuthDeg;
    m_hasSweepRd = true;
    if (!m_videoTimer->isActive()) {
        m_fadeClock.start();
        m_videoTimer->start();
    }
}

void MapView::setMhSweep(double azimuthDeg, const QByteArray &video)
{
    m_sweepMh = azimuthDeg;
    m_hasSweepMh = true;
    if (video.size() >= Video::kSamples) {
        if (m_pendingSpokes.size() >= kMaxPendingSpokes)
            m_pendingSpokes.remove(0, m_pendingSpokes.size() - kMaxPendingSpokes + 1);
        m_pendingSpokes.append(Spoke{azimuthDeg, video});
    }
    if (!m_videoTimer->isActive()) {
        m_fadeClock.start();
        m_videoTimer->start();
    }
}

void MapView::clearVideo()
{
    m_videoTimer->stop();
    m_pendingSpokes.clear();
    m_hasSweepRd = false;
    m_hasSweepMh = false;
    if (!m_videoLayer.isNull())
        m_videoLayer.fill(Qt::transparent);
    m_fadeCarry = 0.0;
    update();
}

void MapView::ensureVideoLayer()
{
    const qreal dpr = devicePixelRatioF();
    const QSize want(qRound(width() * dpr), qRound(height() * dpr));
    if (m_videoLayer.size() == want)
        return;
    m_videoLayer = QImage(want, QImage::Format_ARGB32_Premultiplied);
    m_videoLayer.setDevicePixelRatio(dpr);
    m_videoLayer.fill(Qt::transparent);
    m_fadeCarry = 0.0;
}

void MapView::fadeVideoLayer(double seconds)
{
    const int fadeSec = Settings::instance().setups().videoFade;
    if (fadeSec <= 0) {
        m_videoLayer.fill(Qt::transparent);
        m_fadeCarry = 0.0;
        return;
    }

    // Trừ dần mức alpha thay vì nhân hệ số: phép nhân số nguyên đứng lại ở các
    // mức alpha thấp và để lại một lớp xanh mờ không bao giờ tắt. Phần lẻ chưa
    // trừ hết được dồn sang lần sau nên tổng thời gian tắt đúng bằng thanh
    // trượt "Tốc độ mờ video".
    m_fadeCarry += 255.0 * seconds / fadeSec;
    const int step = int(m_fadeCarry);
    if (step <= 0)
        return;
    m_fadeCarry -= step;

    const int h = m_videoLayer.height();
    const int w = m_videoLayer.width();
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(m_videoLayer.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = qAlpha(line[x]);
            if (a == 0)
                continue;
            const int na = a > step ? a - step : 0;
            // Lớp video chỉ có màu xanh lá thuần nên giá trị nhân sẵn alpha của
            // kênh lục luôn bằng chính alpha.
            line[x] = qRgba(0, na, 0, na);
        }
    }
}

void MapView::flushVideo()
{
    ensureVideoLayer();
    const double dt = m_fadeClock.isValid() ? m_fadeClock.restart() / 1000.0
                                            : kVideoFrameMs / 1000.0;

    if (!m_pendingSpokes.isEmpty()) {
        QPainter p(&m_videoLayer);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QPointF center = viewTransform().map(QPointF(0.0, 0.0));
        const double radiusPx = kMaxRangeKm * m_scale;

        // Một tia = ảnh 600x1 điểm, vẽ qua phép biến đổi quay + giãn. Rẻ hơn
        // nhiều so với 600 đoạn thẳng cho mỗi tia, và Qt nội suy sẵn nên ảnh
        // mượt ở mọi mức zoom.
        QImage spoke(Video::kSamples, 1, QImage::Format_ARGB32_Premultiplied);
        for (const Spoke &s : std::as_const(m_pendingSpokes)) {
            QRgb *px = reinterpret_cast<QRgb *>(spoke.bits());
            const auto *v = reinterpret_cast<const quint8 *>(s.video.constData());
            for (int i = 0; i < Video::kSamples; ++i)
                px[i] = qRgba(0, v[i], 0, v[i]);

            QTransform t;
            t.translate(center.x(), center.y());
            t.rotate(s.az - 90.0);   // 0 độ là hướng bắc, trục x của ảnh hướng đông
            t.scale(radiusPx / Video::kSamples, 1.0);
            p.setTransform(t);
            p.drawImage(QRectF(0.0, -0.75, Video::kSamples, 1.5), spoke);
        }
        m_pendingSpokes.clear();
    }

    fadeVideoLayer(dt);
    update();
}

void MapView::drawSweepLines(QPainter &p)
{
    if (!m_hasSweepRd && !m_hasSweepMh)
        return;

    const QPointF center = viewTransform().map(QPointF(0.0, 0.0));
    const double radiusPx = kMaxRangeKm * m_scale;

    p.setRenderHint(QPainter::Antialiasing, true);
    const auto sweep = [&](double azDeg, const QColor &color) {
        const double a = azDeg * M_PI / 180.0;
        p.setPen(QPen(color, 1.4));
        p.drawLine(center, center + QPointF(std::sin(a) * radiusPx, -std::cos(a) * radiusPx));
    };
    if (m_hasSweepRd)
        sweep(m_sweepRd, kSweepRdColor);
    if (m_hasSweepMh)
        sweep(m_sweepMh, kSweepMhColor);
}

void MapView::rebuildCache()
{
    const qreal dpr = devicePixelRatioF();
    m_cache = QPixmap(QSize(qRound(width() * dpr), qRound(height() * dpr)));
    m_cache.setDevicePixelRatio(dpr);

    const Setups &st = Settings::instance().setups();
    m_cache.fill(st.showMap ? m_palette.land : QColor(Qt::black));

    QPainter p(&m_cache);
    p.setRenderHint(QPainter::Antialiasing, true);

    bool ok = false;
    const QTransform t = viewTransform();
    const QTransform inv = t.inverted(&ok);
    if (!ok) {
        m_cacheDirty = false;
        return;
    }
    const QRectF viewPlane = inv.mapRect(QRectF(rect())).adjusted(-2, -2, 2, 2);
    const int lod = MapData::lodForScale(m_scale);

    if (st.showMap) {
        p.save();
        p.setTransform(t);
        drawLayers(p, viewPlane, lod);
        p.restore();
        drawPoints(p, viewPlane);
    }

    drawGrid(p, viewPlane);

    p.end();
    m_cacheDirty = false;
}

void MapView::drawLayers(QPainter &p, const QRectF &viewPlane, int lod)
{
    if (!m_data)
        return;
    const Setups &st = Settings::instance().setups();

    for (const MapLayer &layer : m_data->layers()) {
        if (layer.id == LayerId::AirRoutes && !st.showAirRoutes)
            continue;

        QPen pen(Qt::NoPen);
        QBrush brush(Qt::NoBrush);
        switch (layer.id) {
        case LayerId::NationalArea:
            brush = m_palette.sea;
            break;
        case LayerId::LandIslands:
            brush = m_palette.land;
            pen = QPen(m_palette.coast, 1.0);
            break;
        case LayerId::Provinces:
            brush = m_palette.land;
            pen = QPen(m_palette.province, 1.0);
            break;
        case LayerId::Rivers:
            brush = m_palette.river;
            pen = QPen(m_palette.river, 1.0);
            break;
        case LayerId::CoastLines:
        case LayerId::BorderLines:
            pen = QPen(m_palette.coast, layer.id == LayerId::BorderLines ? 1.4 : 1.0);
            break;
        case LayerId::HoangSa:
        case LayerId::TruongSa:
            pen = QPen(m_palette.coast, 1.0);
            break;
        case LayerId::AirRoutes:
            pen = QPen(m_palette.airRoute, 1.0);
            break;
        default:
            break;
        }
        // Nét vẽ tính theo pixel nên không dày lên khi phóng to bản đồ.
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(brush);

        for (const MapFeature &f : layer.features) {
            if (!f.bbox.intersects(viewPlane))
                continue;
            if (layer.filled) {
                p.drawPath(f.lodPath[lod]);
            } else {
                for (const QPolygonF &poly : f.lodLines[lod])
                    p.drawPolyline(poly);
            }
        }
    }
}

void MapView::drawPoints(QPainter &p, const QRectF &viewPlane)
{
    if (!m_data)
        return;
    const Setups &st = Settings::instance().setups();
    const QTransform t = viewTransform();

    QFont labelFont = font();
    labelFont.setPointSizeF(qMax(7.0, basePointSize(font()) - 1.0));
    p.setFont(labelFont);
    const QFontMetricsF fm(labelFont);

    // Địa danh: chấm nhỏ kèm tên, chỉ hiện khi đã zoom đủ để chữ không chồng nhau.
    if (m_scale > kLabelScale) {
        p.setPen(m_palette.placeText);
        for (const PlaceLabel &pl : m_data->places()) {
            if (!viewPlane.contains(pl.plane))
                continue;
            const QPointF s = t.map(pl.plane);
            p.setBrush(m_palette.placeText);
            p.drawEllipse(s, 2.0, 2.0);
            p.drawText(QPointF(s.x() - fm.horizontalAdvance(pl.name) / 2.0, s.y() - 5.0), pl.name);
        }
    }

    // Nhãn đường bay dân dụng.
    if (st.showAirRoutes && m_scale > kLabelScale) {
        p.setPen(m_palette.routeText);
        p.setBrush(Qt::NoBrush);
        for (const RouteLabel &r : m_data->routeLabels()) {
            if (!viewPlane.contains(r.plane))
                continue;
            const QPointF s = t.map(r.plane);
            p.drawText(QPointF(s.x() - fm.horizontalAdvance(r.name) / 2.0, s.y() - 3.0), r.name);
        }
    }

    if (!st.showAirports)
        return;

    // Ký hiệu sân bay: cỡ cố định theo pixel để luôn đọc được ở mọi mức zoom.
    const double symbolSize = 16.0;
    for (const AirportInfo &a : m_data->airports()) {
        if (!viewPlane.contains(a.plane))
            continue;
        const QPointF s = t.map(a.plane);
        const bool foreign = (a.level <= 0);
        IconFactory::drawAirport(&p, s, a.level, a.heading, symbolSize,
                                 foreign ? m_palette.airportForeign : m_palette.airportVn);
        if (m_scale <= kLabelScale)
            continue;
        p.setPen(foreign ? m_palette.airportTextForeign : m_palette.airportTextVn);
        const QString name = QStringLiteral("SB ") + a.name;
        p.drawText(QPointF(s.x() - fm.horizontalAdvance(name) / 2.0, s.y() + symbolSize * 0.72 + fm.ascent()),
                   name);
    }
}

void MapView::drawGrid(QPainter &p, const QRectF &viewPlane)
{
    const Setups &st = Settings::instance().setups();
    if (st.rangeRingMode >= 3 && st.azimuthMode >= 3)
        return;

    const QTransform t = viewTransform();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);

    QColor line = st.colors.grid;
    line.setAlpha(115); // lưới phải đọc được nhưng không lấn nền bản đồ

    // ------------------------------------------------ vòng cự ly
    if (st.rangeRingMode < 3) {
        // Bước nhỏ nhất theo lựa chọn; vòng chia hết cho mốc lớn hơn vẽ đậm hơn.
        const int step = (st.rangeRingMode == 0) ? 50 : (st.rangeRingMode == 1 ? 10 : 5);
        for (int r = step; r <= int(kMaxRangeKm); r += step) {
            double w = 0.6;
            if (r % 50 == 0)
                w = 1.3;
            else if (r % 10 == 0)
                w = 0.9;
            QPen pen(line, w);
            pen.setCosmetic(true);
            p.setPen(pen);
            const QRectF ring(-r, -r, 2.0 * r, 2.0 * r);
            p.drawEllipse(t.mapRect(ring));
        }
        // Vòng cự ly tối đa luôn có mặt dù 360 không chia hết cho bước đang chọn.
        QPen pen(line, 1.6);
        pen.setCosmetic(true);
        p.setPen(pen);
        const QRectF ring(-kMaxRangeKm, -kMaxRangeKm, 2.0 * kMaxRangeKm, 2.0 * kMaxRangeKm);
        p.drawEllipse(t.mapRect(ring));
    }

    // ------------------------------------------------ đường chia độ
    if (st.azimuthMode < 3) {
        const int step = (st.azimuthMode == 0) ? 30 : (st.azimuthMode == 1 ? 10 : 5);
        for (int a = 0; a < 360; a += step) {
            double w = 0.6;
            if (a % 30 == 0)
                w = 1.1;
            else if (a % 10 == 0)
                w = 0.8;
            QPen pen(line, w);
            pen.setCosmetic(true);
            p.setPen(pen);
            p.drawLine(t.map(QPointF(0, 0)),
                       t.map(LocalProjection::planeFromPolar(a, kMaxRangeKm)));
        }
    }

    // ------------------------------------------------ số cự ly trên trục bắc-nam
    if (st.rangeRingMode < 3) {
        QColor textColor = st.colors.grid;
        textColor.setAlpha(200);
        p.setPen(textColor);
        QFont f = font();
        f.setPointSizeF(qMax(7.0, basePointSize(font()) - 1.0));
        p.setFont(f);
        const QFontMetricsF fm(f);
        for (int r = 50; r <= int(kMaxRangeKm); r += 50) {
            const QString s = QString::number(r);
            const double w = fm.horizontalAdvance(s);
            for (int sign = -1; sign <= 1; sign += 2) {
                const QPointF pt = t.map(QPointF(0, sign * double(r)));
                if (!rect().contains(pt.toPoint()))
                    continue;
                p.drawText(QPointF(pt.x() + 5.0, pt.y() - w * 0.0 + fm.ascent() * 0.35), s);
            }
        }
    }

    // ------------------------------------------------ ký hiệu tâm đài
    IconFactory::drawRadarSite(&p, t.map(QPointF(0, 0)), 22.0, st.colors.grid);

    p.restore();
}

void MapView::drawInfoBox(QPainter &p)
{
    const SwInfo &info = Settings::instance().swInfo();

    QFont f1 = font();
    f1.setBold(true);
    f1.setPointSizeF(basePointSize(font()) + 1.0);
    QFont f2 = font();

    const QFontMetricsF fm1(f1), fm2(f2);
    const double logoSize = 30.0;
    const double pad = 7.0;
    const double gap = 7.0;

    const double textW = qMax(fm1.horizontalAdvance(info.line1), fm2.horizontalAdvance(info.line2));
    const double textH = fm1.height() + fm2.height() + 1.0;
    const double boxW = pad * 2 + (m_logo.isNull() ? 0.0 : logoSize + gap) + textW;
    const double boxH = pad * 2 + qMax(textH, m_logo.isNull() ? 0.0 : logoSize);
    const QRectF box(8, 8, boxW, boxH);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0x14, 0x14, 0x16, 205));
    p.setPen(QPen(QColor(0xe0, 0x3a, 0xa0), 1.5)); // khung hồng như ảnh mẫu sw_version.png
    p.drawRoundedRect(box, 3, 3);

    double x = box.left() + pad;
    if (!m_logo.isNull()) {
        const QRectF lr(x, box.top() + (box.height() - logoSize) / 2.0, logoSize, logoSize);
        p.drawPixmap(lr.toRect(), m_logo);
        x += logoSize + gap;
    }

    const double ty = box.top() + (box.height() - textH) / 2.0;
    p.setFont(f1);
    p.setPen(QColor(0xf5, 0xe6, 0xa8));
    p.drawText(QPointF(x, ty + fm1.ascent()), info.line1.toUpper());
    p.setFont(f2);
    p.setPen(QColor(0xe8, 0xee, 0xf4));
    p.drawText(QPointF(x, ty + fm1.height() + fm2.ascent()), info.line2);
    p.restore();
}
