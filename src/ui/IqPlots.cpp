#include "ui/IqPlots.h"

#include "proto/RawIq.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

// MSVC không định nghĩa M_PI nếu thiếu _USE_MATH_DEFINES; khai báo thẳng cho gọn.
constexpr double kPi = 3.14159265358979323846;

const QColor kGrid(0x26, 0x2d, 0x35);
const QColor kGridStrong(0x3a, 0x44, 0x4f);
const QColor kAxisText(0x8a, 0x95, 0xa1);
const QColor kHint(0x4a, 0x55, 0x60);
const QColor kRed(0xff, 0x4d, 0x4d);
const QColor kBlue(0x3f, 0xa9, 0xf5);
const QColor kSweep(0x3c, 0xff, 0x6a);
const QColor kHover(0xff, 0xd2, 0x4d);

// Lề chừa cho nhãn trục Y ("-32768") và nhãn trục X.
constexpr double kLeftMargin = 46.0;
constexpr double kBottomMargin = 16.0;

// Phần chừa trên/dưới đường cong khi co giãn thang, và tốc độ co lại mỗi nhịp
// vẽ: 4% mỗi 40 ms, tức thang về sát dữ liệu sau chừng 3 giây.
constexpr double kPad = 0.08;
constexpr double kShrink = 0.04;

// Nét của mọi đường dữ liệu phải đúng 1 px. Bút dày hơn (dù chỉ 1,3 px) làm
// QPainter bỏ đường vẽ nhanh (cosmetic stroker) mà dựng viền cả polyline rồi
// mới tô: đo được 100 ms/khung cho 2 x 4096 điểm cánh sóng thay vì 1 ms, tức
// luồng giao diện nghẽn hẳn ở nhịp 40 ms.
constexpr double kTraceWidth = 1.0;

// Hai ô phương vị có số liệu cách nhau quá 64 ô (5,6°) thì không nối: khoảng
// trống đó là chỗ ăng ten chưa quét tới từ lúc Start, không phải cánh sóng.
constexpr int kMaxGapBins = 64;

QFont smallFont(const QWidget *w)
{
    QFont f = w->font();
    f.setPointSizeF(qMax(6.5, f.pointSizeF() - 1.5));
    return f;
}

// Bước chia 1-2-5 × 10^n sao cho khoảng [0, span] có không quá maxTicks vạch.
double niceStep(double span, int maxTicks)
{
    if (!(span > 0.0))
        return 1.0;
    const double raw = span / qMax(1, maxTicks);
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    const double step = norm <= 1.0 ? 1.0 : norm <= 2.0 ? 2.0 : norm <= 5.0 ? 5.0 : 10.0;
    return step * mag;
}

QString tickText(double v, double step)
{
    if (std::fabs(v) < step * 1e-6)
        v = 0.0;          // tránh nhãn "-0"
    const int decimals = step >= 1.0 ? 0 : int(std::ceil(-std::log10(step) - 1e-9));
    return QString::number(v, 'f', decimals);
}

// Gọi fn(v) cho từng vạch chia trong [lo, hi].
template <typename Fn>
void forEachTick(double lo, double hi, double step, Fn fn)
{
    const double first = std::ceil(lo / step - 1e-9) * step;
    for (int k = 0; k < 64; ++k) {
        const double v = first + k * step;
        if (v > hi + step * 1e-9)
            break;
        fn(v);
    }
}

// Lưới ngang + nhãn trục Y bên trái vùng vẽ.
void drawYAxis(QPainter &p, const QRectF &area, double lo, double hi)
{
    const double step = niceStep(hi - lo, qBound(2, int(area.height() / 36), 10));
    const QFontMetrics fm = p.fontMetrics();
    forEachTick(lo, hi, step, [&](double v) {
        const double y = area.bottom() - (v - lo) / (hi - lo) * area.height();
        p.setPen(QPen(std::fabs(v) < step * 1e-6 ? kGridStrong : kGrid, 1));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        p.setPen(kAxisText);
        p.drawText(QRectF(0, y - fm.height() / 2.0, area.left() - 4, fm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, tickText(v, step));
    });
}

// Nhãn trục X canh giữa vạch chia nhưng không tràn khỏi mép widget: nhãn cuối
// ("2600", "360") nằm sát mép phải, canh giữa thì bị cắt mất nửa.
void drawXLabel(QPainter &p, double x, double top, double maxRight, const QString &text)
{
    const QFontMetrics fm = p.fontMetrics();
    const double w = fm.horizontalAdvance(text);
    const double left = qBound(0.0, x - w / 2.0, maxRight - w);
    p.drawText(QPointF(left, top + fm.ascent()), text);
}

// Chú thích hai đường ở góc trên bên phải.
void drawLegend(QPainter &p, const QRectF &area, const QString &a, const QString &b)
{
    const QFontMetrics fm = p.fontMetrics();
    const double h = fm.height();
    double x = area.right() - 8;
    const QString names[2] = {b, a};
    const QColor colors[2] = {kBlue, kRed};
    for (int i = 0; i < 2; ++i) {
        const double w = fm.horizontalAdvance(names[i]);
        x -= w;
        p.setPen(kAxisText);
        p.drawText(QPointF(x, area.top() + 4 + fm.ascent()), names[i]);
        x -= 20;
        p.setPen(QPen(colors[i], 2));
        p.drawLine(QPointF(x, area.top() + 4 + h / 2), QPointF(x + 16, area.top() + 4 + h / 2));
        x -= 12;
    }
}

// Ô chữ khi di chuột, bám theo đường dóng dọc nhưng không tràn ra ngoài.
void drawHoverBox(QPainter &p, const QRectF &area, double x, const QString &text)
{
    p.setPen(QPen(kHover, 1, Qt::DashLine));
    p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));

    const QFontMetrics fm = p.fontMetrics();
    const double w = fm.horizontalAdvance(text) + 12;
    const double h = fm.height() + 4;
    const double left = (x + 6 + w <= area.right()) ? x + 6 : qMax(area.left(), x - 6 - w);
    const QRectF box(left, area.top() + fm.height() + 10, w, h);
    p.fillRect(box, QColor(0, 0, 0, 200));
    p.setPen(kHover);
    p.drawText(box, Qt::AlignCenter, text);
}

void drawHint(QPainter &p, const QRectF &area, const QString &text)
{
    p.setPen(kHint);
    p.drawText(area, Qt::AlignCenter, text);
}

// Tách chuỗi 4096 ô phương vị thành các đoạn nối liền.
//
// Duyệt bắt đầu ngay sau đường quét nên nét cũ nhất đứng đầu và nét mới nhất
// ở cuối: đường không bao giờ nối vòng qua chỗ đường quét sắp đi tới, giống
// như vệt cánh sóng được vẽ đồng bộ theo đường quét. breakAtWrap cắt thêm ở
// chỗ 360° → 0° cho đồ thị trục ngang.
template <typename Map>
QVector<QPolygonF> traceSegments(const std::vector<double> &v, int sweep, bool breakAtWrap, Map map)
{
    QVector<QPolygonF> out;
    const int n = int(v.size());
    if (n == 0)
        return out;
    const int first = (sweep >= 0) ? (sweep + 1) % n : 0;
    QPolygonF cur;
    int lastK = 0;
    for (int k = 0; k < n; ++k) {
        const int i = (first + k) % n;
        if (breakAtWrap && i == 0 && !cur.isEmpty()) {
            out.append(cur);
            cur.clear();
        }
        const double y = v[size_t(i)];
        if (std::isnan(y))
            continue;
        if (!cur.isEmpty() && k - lastK > kMaxGapBins) {
            out.append(cur);
            cur.clear();
        }
        cur.append(map(i, y));
        lastK = k;
    }
    if (!cur.isEmpty())
        out.append(cur);
    return out;
}

void drawSegments(QPainter &p, const QVector<QPolygonF> &segments, const QColor &color)
{
    p.setPen(QPen(color, kTraceWidth));
    for (const QPolygonF &s : segments) {
        if (s.size() >= 2) {
            p.drawPolyline(s);
        } else {
            // Điểm lẻ loi (ăng ten quay nhanh, gói thưa) vẫn phải thấy được.
            p.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap));
            p.drawPoint(s.first());
            p.setPen(QPen(color, kTraceWidth));
        }
    }
}

QString valueText(double v)
{
    return std::isnan(v) ? QStringLiteral("—") : QString::number(v, 'f', 2);
}

} // namespace

// -------------------------------------------------------------- AutoRange

void AutoRange::feed(double lo, double hi, bool floorAtZero)
{
    if (!(lo <= hi))
        return;               // NaN hoặc chưa có dữ liệu
    const bool nonNegative = floorAtZero && lo >= 0.0;

    double span = hi - lo;
    if (span < m_minSpan) {
        const double mid = (lo + hi) / 2.0;
        lo = mid - m_minSpan / 2.0;
        hi = mid + m_minSpan / 2.0;
        span = m_minSpan;
    }
    double targetLo = lo - span * kPad;
    double targetHi = hi + span * kPad;
    if (nonNegative && targetLo < 0.0) {
        targetHi -= targetLo;
        targetLo = 0.0;
    }

    if (!m_valid) {
        m_lo = targetLo;
        m_hi = targetHi;
        m_valid = true;
        return;
    }
    m_lo = targetLo < m_lo ? targetLo : m_lo + (targetLo - m_lo) * kShrink;
    m_hi = targetHi > m_hi ? targetHi : m_hi + (targetHi - m_hi) * kShrink;
}

// ------------------------------------------------------------ IqScopeView

IqScopeView::IqScopeView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 90);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
}

void IqScopeView::setTrace(int startWord, const std::vector<qint16> &iq1,
                           const std::vector<qint16> &iq2)
{
    m_start = startWord;
    m_iq1 = iq1;
    m_iq2 = iq2;
    if (!m_iq1.empty() && m_iq1.size() == m_iq2.size()) {
        const auto [lo1, hi1] = std::minmax_element(m_iq1.begin(), m_iq1.end());
        const auto [lo2, hi2] = std::minmax_element(m_iq2.begin(), m_iq2.end());
        // Tín hiệu IQ dao động quanh 0 nên giữ vạch 0 luôn nằm trong khung.
        const double lo = qMin(0.0, double(qMin(*lo1, *lo2)));
        const double hi = qMax(0.0, double(qMax(*hi1, *hi2)));
        m_range.feed(lo, hi, false);
    }
    update();
}

void IqScopeView::clearTrace()
{
    m_iq1.clear();
    m_iq2.clear();
    m_range.reset();
    update();
}

QRectF IqScopeView::plotArea() const
{
    return QRectF(rect()).adjusted(kLeftMargin, 6, -8, -kBottomMargin);
}

void IqScopeView::mouseMoveEvent(QMouseEvent *event)
{
    m_cursor = event->pos();
    m_hasCursor = plotArea().contains(m_cursor);
    update();
}

void IqScopeView::leaveEvent(QEvent *event)
{
    m_hasCursor = false;
    update();
    QWidget::leaveEvent(event);
}

void IqScopeView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setFont(smallFont(this));

    const QRectF area = plotArea();
    if (area.width() < 20 || area.height() < 20)
        return;

    // Trục ngang: chỉ số word StartWord..StartWord + 600.
    const int n = RawIq::kViewPoints;
    const double xStep = niceStep(n, qBound(2, int(area.width() / 70), 12));
    forEachTick(m_start, m_start + n, xStep, [&](double w) {
        const double x = area.left() + (w - m_start) / n * area.width();
        p.setPen(QPen(kGrid, 1));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        p.setPen(kAxisText);
        drawXLabel(p, x, area.bottom() + 1, width() - 1, tickText(w, xStep));
    });

    const bool hasData = m_iq1.size() == size_t(n) && m_iq2.size() == size_t(n) && m_range.valid();
    if (!hasData) {
        p.setPen(QPen(kGridStrong, 1));
        p.drawRect(area);
        drawHint(p, area, QStringLiteral("Chưa có dữ liệu ViewIQ"));
        return;
    }

    const double lo = m_range.lo();
    const double hi = m_range.hi();
    drawYAxis(p, area, lo, hi);
    p.setPen(QPen(kGridStrong, 1));
    p.drawRect(area);

    const double dx = area.width() / n;
    const double sy = area.height() / (hi - lo);
    QPolygonF poly1(n);
    QPolygonF poly2(n);
    for (int i = 0; i < n; ++i) {
        const double x = area.left() + dx * i;
        poly1[i] = QPointF(x, area.bottom() - (m_iq1[size_t(i)] - lo) * sy);
        poly2[i] = QPointF(x, area.bottom() - (m_iq2[size_t(i)] - lo) * sy);
    }
    p.save();
    p.setClipRect(area.adjusted(-1, -1, 1, 1));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(kRed, kTraceWidth));
    p.drawPolyline(poly1);
    p.setPen(QPen(kBlue, kTraceWidth));
    p.drawPolyline(poly2);
    p.restore();

    drawLegend(p, area, QStringLiteral("IQ1"), QStringLiteral("IQ2"));

    if (!m_hasCursor)
        return;
    const int i = qBound(0, int((m_cursor.x() - area.left()) / dx + 0.5), n - 1);
    drawHoverBox(p, area, area.left() + dx * i,
                 QStringLiteral("word %1:  %2 - %3")
                     .arg(m_start + i)
                     .arg(m_iq1[size_t(i)])
                     .arg(m_iq2[size_t(i)]));
}

// ----------------------------------------------------------- BeamPolarView

BeamPolarView::BeamPolarView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(120, 120);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void BeamPolarView::setTrace(const BeamTrace &trace)
{
    m_trace = trace;
    update();
}

void BeamPolarView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setFont(smallFont(this));
    p.setRenderHint(QPainter::Antialiasing, true);

    const QFontMetrics fm = p.fontMetrics();
    const double labelPad = fm.height() + 4;
    const QRectF bounds = QRectF(rect()).adjusted(4, 4, -4, -4);
    const double radius = qMin(bounds.width(), bounds.height()) / 2.0 - labelPad;
    if (radius < 20)
        return;
    const QPointF c = bounds.center();

    // Hướng của phương vị a (độ): 0° lên trên, tăng theo chiều kim đồng hồ.
    const auto dir = [](double deg) {
        const double r = deg * kPi / 180.0;
        return QPointF(std::sin(r), -std::cos(r));
    };

    // Vạch phương vị mỗi 30°.
    for (int a = 0; a < 360; a += 30) {
        const QPointF d = dir(a);
        p.setPen(QPen(kGrid, 1));
        p.drawLine(c, c + d * radius);
        p.setPen(kAxisText);
        const QPointF t = c + d * (radius + labelPad / 2.0 + 2);
        p.drawText(QRectF(t.x() - 20, t.y() - fm.height() / 2.0, 40, fm.height()),
                   Qt::AlignCenter, QString::number(a));
    }

    const double lo = m_trace.lo;
    const double hi = m_trace.hi;
    if (m_trace.valid) {
        // Vòng tròn biên độ đúng các vạch chia trục Y của đồ thị bên phải.
        const double step = niceStep(hi - lo, qBound(2, int(radius / 36), 8));
        forEachTick(lo, hi, step, [&](double v) {
            const double r = (v - lo) / (hi - lo) * radius;
            if (r < 2)
                return;
            p.setPen(QPen(kGrid, 1));
            p.drawEllipse(c, r, r);
            // Vòng sát mép ngoài thì bỏ nhãn: chữ sẽ đè lên nhãn phương vị "0".
            if (r > radius - fm.height())
                return;
            p.setPen(kAxisText);
            p.drawText(QPointF(c.x() + 3, c.y() - r - 2), tickText(v, step));
        });
    }
    p.setPen(QPen(kGridStrong, 1));
    p.drawEllipse(c, radius, radius);

    if (m_trace.valid) {
        const double scale = radius / (hi - lo);
        const auto map = [&](int bin, double v) {
            const double r = qBound(0.0, (v - lo) * scale, radius);
            return c + dir(RawIq::azimuthDeg(bin)) * r;
        };
        drawSegments(p, traceSegments(m_trace.sum, m_trace.sweep, false, map), kRed);
        drawSegments(p, traceSegments(m_trace.sub, m_trace.sweep, false, map), kBlue);
    } else {
        drawHint(p, QRectF(c.x() - radius, c.y() + radius / 3, 2 * radius, fm.height() * 2),
                 QStringLiteral("Chưa có dữ liệu Vẽ CS"));
    }

    if (m_trace.sweep >= 0) {
        p.setPen(QPen(kSweep, 1.5));
        p.drawLine(c, c + dir(RawIq::azimuthDeg(m_trace.sweep)) * radius);
    }
}

// ----------------------------------------------------------- BeamScopeView

BeamScopeView::BeamScopeView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 90);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
}

void BeamScopeView::setTrace(const BeamTrace &trace)
{
    m_trace = trace;
    update();
}

QRectF BeamScopeView::plotArea() const
{
    return QRectF(rect()).adjusted(kLeftMargin, 6, -8, -kBottomMargin);
}

void BeamScopeView::mouseMoveEvent(QMouseEvent *event)
{
    m_cursor = event->pos();
    m_hasCursor = plotArea().contains(m_cursor);
    update();
}

void BeamScopeView::leaveEvent(QEvent *event)
{
    m_hasCursor = false;
    update();
    QWidget::leaveEvent(event);
}

void BeamScopeView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setFont(smallFont(this));

    const QRectF area = plotArea();
    if (area.width() < 20 || area.height() < 20)
        return;

    // Trục ngang 0..360°: thưa vạch dần khi đồ thị hẹp lại.
    const int azStep = area.width() < 220 ? 90 : area.width() < 400 ? 45 : 30;
    for (int a = 0; a <= 360; a += azStep) {
        const double x = area.left() + a / 360.0 * area.width();
        p.setPen(QPen(kGrid, 1));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        p.setPen(kAxisText);
        drawXLabel(p, x, area.bottom() + 1, width() - 1, QString::number(a));
    }

    const double binW = area.width() / RawIq::kAzimuthSteps;
    if (m_trace.sweep >= 0) {
        const double x = area.left() + (m_trace.sweep + 0.5) * binW;
        p.setPen(QPen(kSweep, 1));
        p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
    }

    const size_t bins = size_t(RawIq::kAzimuthSteps);
    if (!m_trace.valid || m_trace.sum.size() != bins || m_trace.sub.size() != bins) {
        p.setPen(QPen(kGridStrong, 1));
        p.drawRect(area);
        drawHint(p, area, QStringLiteral("Chưa có dữ liệu Vẽ CS"));
        return;
    }

    const double lo = m_trace.lo;
    const double hi = m_trace.hi;
    drawYAxis(p, area, lo, hi);
    p.setPen(QPen(kGridStrong, 1));
    p.drawRect(area);

    const double sy = area.height() / (hi - lo);
    const auto map = [&](int bin, double v) {
        return QPointF(area.left() + (bin + 0.5) * binW, area.bottom() - (v - lo) * sy);
    };
    p.save();
    p.setClipRect(area.adjusted(-1, -1, 1, 1));
    p.setRenderHint(QPainter::Antialiasing, true);
    drawSegments(p, traceSegments(m_trace.sum, m_trace.sweep, true, map), kRed);
    drawSegments(p, traceSegments(m_trace.sub, m_trace.sweep, true, map), kBlue);
    p.restore();

    drawLegend(p, area, QStringLiteral("Sum"), QStringLiteral("Sub"));

    if (!m_hasCursor)
        return;

    // Ô phương vị dưới con trỏ thường trống (mỗi gói nhảy vài ô), nên tìm ô
    // có số liệu gần nhất trong phạm vi 4 px.
    const int n = RawIq::kAzimuthSteps;
    const int wanted = qBound(0, int((m_cursor.x() - area.left()) / binW), n - 1);
    const int reach = qMax(2, int(std::ceil(4.0 / binW)));
    int found = -1;
    for (int d = 0; d <= reach && found < 0; ++d) {
        for (const int bin : {wanted - d, wanted + d}) {
            const size_t k = size_t((bin + n) % n);
            if (!std::isnan(m_trace.sum[k]) || !std::isnan(m_trace.sub[k])) {
                found = int(k);
                break;
            }
        }
    }
    const int bin = found >= 0 ? found : wanted;
    const QString az = QString::number(RawIq::azimuthDeg(bin), 'f', 2) + QStringLiteral("°");
    const QString text = found >= 0
        ? QStringLiteral("%1:  %2 - %3").arg(az, valueText(m_trace.sum[size_t(bin)]),
                                             valueText(m_trace.sub[size_t(bin)]))
        : az;
    drawHoverBox(p, area, area.left() + (bin + 0.5) * binW, text);
}
