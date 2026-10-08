#include "ui/TargetLayer.h"

#include "core/GeoCalc.h"
#include "core/Settings.h"
#include "track/TrackStore.h"
#include "ui/IconFactory.h"

#include <QFontInfo>
#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace IffText {

QString mode(quint32 retmode, bool *bold)
{
    if (bold)
        *bold = (retmode >= 7 && retmode <= 9);
    switch (retmode) {
    case 1: case 2: case 3: case 4: case 5: case 6:
        return QStringLiteral("Chế độ %1").arg(retmode);
    case 7:
        return QStringLiteral("BÁO ĐỘNG");
    case 8:
        return QStringLiteral("Báo nạn CĐ 1");
    case 9:
        return QStringLiteral("Báo nạn CĐ 2");
    default:
        return QString();
    }
}

QStringList details(quint32 retmode, quint32 commander, quint32 flightid,
                    quint32 altitude, quint32 fuel)
{
    return held(retmode == 3 ? commander : 0, retmode == 4 ? flightid : 0,
                retmode == 6 ? altitude : 0, retmode == 6 ? fuel : 0);
}

QStringList held(quint32 commander, quint32 flightid, quint32 altitude, quint32 fuel)
{
    QStringList out;
    if (commander == 1)
        out << QStringLiteral("Tốp chỉ huy");
    if (flightid > 0)
        out << QStringLiteral("Số hiệu: %1").arg(flightid);
    if (altitude > 0)
        out << QStringLiteral("Độ cao: %1m").arg(altitude);
    if (fuel > 0)
        out << QStringLiteral("Nhiên liệu: %1%").arg(fuel);
    return out;
}

} // namespace IffText

namespace {

// Cỡ ở mức 100%: ký hiệu quỹ đạo cỡ ký hiệu sân bay phóng gấp rưỡi, điểm dấu
// MH nhỏ hơn nhiều để không che quỹ đạo nằm cùng chỗ.
constexpr double kTrackSizePx = 24.0;
constexpr double kPlotSizePx = 9.0;

// "Mỗi giây đổi màu 4 lần" (step-06 mục 10).
constexpr qint64 kBlinkMs = 250;

// Chặn số điểm dấu giữ để vẽ: dù hệ thống MH gửi dồn dập thì bộ nhớ và thời
// gian vẽ vẫn có trần.
constexpr int kMaxPlots = 3000;

// Ô thông tin kiểu tooltip: nền sáng trong suốt, chữ tối, viền màu đối tượng.
const QColor kNoteBackground(255, 252, 222, 185);
const QColor kNoteText(0x10, 0x14, 0x18);
constexpr double kNotePad = 4.0;

struct NoteLine {
    QString text;
    bool bold = false;
    bool center = false;
};

QSizeF noteSize(const QVector<NoteLine> &lines, const QFont &font)
{
    QFont bold = font;
    bold.setBold(true);
    const QFontMetricsF fm(font), fmb(bold);
    double w = 0.0;
    for (const NoteLine &l : lines)
        w = qMax(w, (l.bold ? fmb : fm).horizontalAdvance(l.text));
    return QSizeF(std::ceil(w) + 2.0 * kNotePad, lines.size() * fm.height() + 2.0 * kNotePad);
}

void drawNote(QPainter &p, const QRectF &box, const QVector<NoteLine> &lines, const QFont &font,
              const QColor &border)
{
    QFont bold = font;
    bold.setBold(true);
    const double lh = QFontMetricsF(font).height();

    p.setPen(QPen(border, 1.0));
    p.setBrush(kNoteBackground);
    p.drawRect(box);

    p.setPen(kNoteText);
    double y = box.top() + kNotePad;
    for (const NoteLine &l : lines) {
        p.setFont(l.bold ? bold : font);
        const QRectF r(box.left() + kNotePad, y, box.width() - 2.0 * kNotePad, lh);
        p.drawText(r, (l.center ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter, l.text);
        y += lh;
    }
}

// Dòng chế độ phản hồi căn giữa ô, in đậm với báo động / báo nạn.
void appendMode(QVector<NoteLine> *lines, quint32 retmode)
{
    bool bold = false;
    const QString m = IffText::mode(retmode, &bold);
    if (!m.isEmpty())
        lines->append(NoteLine{m, bold, true});
}

// Hộp kích thước size nằm hẳn về phía u (vector đơn vị) của điểm s, cách s ít
// nhất gap: dùng hàm tựa của hình chữ nhật nên hộp không bao giờ đè lên vòng
// tròn bán kính gap quanh s, ở mọi hướng.
QRectF boxToward(const QPointF &s, const QPointF &u, double gap, const QSizeF &size)
{
    const double hw = size.width() / 2.0;
    const double hh = size.height() / 2.0;
    const double reach = gap + hw * std::abs(u.x()) + hh * std::abs(u.y());
    const QPointF c = s + u * reach;
    return QRectF(c.x() - hw, c.y() - hh, size.width(), size.height());
}

// Tâm hộp kích thước size đặt tại want, nằm trên tia từ origin theo dir. Nếu
// hộp tràn ra ngoài view thì lùi tâm dọc theo tia về phía origin cho tới khi
// lọt hẳn vào trong; origin cũng ở ngoài thì đành kẹp thẳng vào khung.
QRectF keepOnRay(const QPointF &origin, const QPointF &dir, const QPointF &want, const QSizeF &size,
                 const QRectF &view)
{
    const double hw = size.width() / 2.0 + 3.0;
    const double hh = size.height() / 2.0 + 3.0;
    const QRectF inner = view.adjusted(hw, hh, -hw, -hh);
    QPointF c = want;
    if (!inner.contains(c) && inner.isValid()) {
        if (inner.contains(origin)) {
            double tMax = QLineF(origin, want).length();
            if (dir.x() > 1e-9)
                tMax = qMin(tMax, (inner.right() - origin.x()) / dir.x());
            else if (dir.x() < -1e-9)
                tMax = qMin(tMax, (inner.left() - origin.x()) / dir.x());
            if (dir.y() > 1e-9)
                tMax = qMin(tMax, (inner.bottom() - origin.y()) / dir.y());
            else if (dir.y() < -1e-9)
                tMax = qMin(tMax, (inner.top() - origin.y()) / dir.y());
            c = origin + dir * qMax(0.0, tMax);
        } else {
            c = QPointF(qBound(inner.left(), c.x(), inner.right()), qBound(inner.top(), c.y(), inner.bottom()));
        }
    }
    return QRectF(c.x() - size.width() / 2.0, c.y() - size.height() / 2.0, size.width(), size.height());
}

// Chữ sáng trên nền bản đồ sáng thì chìm: lót một bóng tối lệch 1 px.
void drawShadowText(QPainter &p, const QPointF &baseline, const QString &text, const QColor &color)
{
    p.setPen(QColor(0, 0, 0, 170));
    p.drawText(baseline + QPointF(1.0, 1.0), text);
    p.setPen(color);
    p.drawText(baseline, text);
}

double basePointSize(const QFont &f)
{
    return f.pointSizeF() > 0.0 ? f.pointSizeF() : QFontInfo(f).pointSizeF();
}

} // namespace

TargetLayer::TargetLayer()
{
    m_clock.start();
}

double TargetLayer::trackSymbolSize()
{
    return kTrackSizePx * Settings::instance().setups().trackSizePct / 100.0;
}

QPointF TargetLayer::trackPlane(const TrackEntry &t)
{
    return LocalProjection::planeFromPolar(t.azimuthDeg(), t.rangeM() / 1000.0);
}

// ------------------------------------------------------------ dữ liệu

void TargetLayer::addPlot(const quint32 *fields)
{
    if (m_plots.size() >= kMaxPlots)
        m_plots.remove(0, int(m_plots.size()) - kMaxPlots + 1);
    PlotMark m;
    std::memcpy(m.f, fields, sizeof(m.f));
    m.plane = LocalProjection::planeFromPolar((fields[Plot::Azm] % 36000u) / 100.0,
                                              fields[Plot::Range] / 1000.0);
    m.ms = m_clock.elapsed();
    m_plots.append(m);
}

void TargetLayer::clearPlots()
{
    m_plots.clear();
}

void TargetLayer::addAlarm(double headDeg)
{
    const qint64 now = m_clock.elapsed();
    for (AlarmRay &a : m_alarms) {
        double d = std::fmod(std::abs(a.headDeg - headDeg), 360.0);
        if (d > 180.0)
            d = 360.0 - d;
        if (d < 0.1) {
            a.ms = now;
            return;
        }
    }
    m_alarms.append(AlarmRay{headDeg, now});
}

void TargetLayer::clearAlarms()
{
    m_alarms.clear();
}

bool TargetLayer::prune()
{
    const qint64 holdMs = qint64(Settings::instance().setups().plotHoldSec) * 1000;
    const qint64 now = m_clock.elapsed();
    // Điểm dấu đến theo thứ tự thời gian nên chỉ cần cắt ở đầu danh sách.
    int old = 0;
    while (old < m_plots.size() && now - m_plots.at(old).ms > holdMs)
        ++old;
    if (old > 0)
        m_plots.remove(0, old);
    m_alarms.erase(std::remove_if(m_alarms.begin(), m_alarms.end(),
                                  [&](const AlarmRay &a) { return now - a.ms > holdMs; }),
                   m_alarms.end());
    return !m_plots.isEmpty() || !m_alarms.isEmpty();
}

bool TargetLayer::trackAt(const QTransform &t, const QPointF &pos, quint32 *id) const
{
    if (!m_tracks)
        return false;
    // Ký hiệu nhỏ (50%) vẫn phải bấm trúng dễ dàng.
    const double radius = qMax(trackSymbolSize() * 0.5, 9.0);
    double best = radius;
    bool hit = false;
    for (const TrackEntry &tr : m_tracks->tracks()) {
        const QPointF s = t.map(trackPlane(tr));
        const double d = std::hypot(s.x() - pos.x(), s.y() - pos.y());
        if (d <= best) {
            best = d;
            *id = tr.id();
            hit = true;
        }
    }
    return hit;
}

// ---------------------------------------------------------------- vẽ

void TargetLayer::draw(QPainter &p, const QTransform &t, double pxPerKm, const QRectF &view,
                       const QFont &font) const
{
    QFont f = font;
    f.setPointSizeF(qMax(7.0, basePointSize(font) - 0.5));

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    drawAlarms(p, t, pxPerKm, view, f);
    drawTracks(p, t, f);
    // Lớp điểm dấu MH luôn nằm trên lớp quỹ đạo (step-06 mục 7).
    drawPlots(p, t, view, f);
    p.restore();
}

void TargetLayer::drawAlarms(QPainter &p, const QTransform &t, double pxPerKm, const QRectF &view,
                             const QFont &font) const
{
    if (m_alarms.isEmpty())
        return;
    const Setups &st = Settings::instance().setups();
    const QColor color = ((m_clock.elapsed() / kBlinkMs) % 2) ? st.colors.alarm2 : st.colors.alarm1;
    const QPointF center = t.map(QPointF(0.0, 0.0));
    const double radius = kMaxRangeKm * pxPerKm;

    QFont bold = font;
    bold.setBold(true);
    p.setFont(bold);
    const QFontMetricsF fm(bold);

    for (const AlarmRay &a : m_alarms) {
        const double rad = qDegreesToRadians(a.headDeg);
        const QPointF dir(std::sin(rad), -std::cos(rad));
        p.setPen(QPen(color, 2.0));
        p.drawLine(center, center + dir * radius);

        // Chữ nằm ngoài vòng cự ly xa nhất, ở đầu tia. Khung nhìn mặc định để
        // vòng 360 km sát mép panel nên chữ ở hướng đông / tây sẽ tràn ra
        // ngoài: khi đó lùi chữ theo tia vào trong cho vẫn đọc được.
        const QString text = QStringLiteral("BĐ %1°").arg(a.headDeg, 0, 'f', 1);
        const QSizeF size(fm.horizontalAdvance(text) + 2.0, fm.height());
        const QRectF want = boxToward(center + dir * radius, dir, 5.0, size);
        const QRectF box = keepOnRay(center, dir, want.center(), size, view);
        drawShadowText(p, QPointF(box.left() + 1.0, box.top() + fm.ascent()), text, color);
    }
}

void TargetLayer::drawTracks(QPainter &p, const QTransform &t, const QFont &font) const
{
    if (!m_tracks || m_tracks->tracks().isEmpty())
        return;
    const Setups &st = Settings::instance().setups();
    const QVector<TrackEntry> &tracks = m_tracks->tracks();
    const double size = trackSymbolSize();

    // ---- vết lịch sử: dưới cùng để không che ký hiệu
    QPen trailPen(st.colors.trackTrail, 1.0);   // đúng 1 px: xem kTraceWidth của IqPlots
    trailPen.setCosmetic(true);
    const double dot = qMax(1.6, size * 0.08);
    QPolygonF pts;
    for (const TrackEntry &tr : tracks) {
        const int total = int(tr.history.size());
        // Quỹ đạo đang theo dõi hiện toàn bộ vết dù tab "Cài đặt" đặt bao nhiêu.
        const int n = tr.followed ? total : qMin(st.trackHistory, total);
        if (n <= 0)
            continue;
        pts.clear();
        pts.reserve(n + 1);
        for (int i = total - n; i < total; ++i) {
            const TrackPoint &h = tr.history.at(i);
            pts.append(t.map(LocalProjection::planeFromPolar(h.azm / 100.0, h.range / 1000.0)));
        }
        if (st.trailStyle == 1) {
            pts.append(t.map(trackPlane(tr)));
            p.setPen(trailPen);
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(pts);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(st.colors.trackTrail);
            for (const QPointF &s : std::as_const(pts))
                p.drawEllipse(s, dot, dot);
        }
    }

    // ---- ký hiệu
    for (const TrackEntry &tr : tracks) {
        const QColor color = tr.type() == Track::TypeVq ? st.colors.track : st.colors.trackMh;
        IconFactory::drawTrack(&p, t.map(trackPlane(tr)), tr.f[Track::Heading] / 100.0, size, color);
    }

    // ---- lý lịch: số tốp ngay trên, phương vị - cự ly ngay dưới ký hiệu
    if (st.showTrackProfile) {
        p.setFont(font);
        const QFontMetricsF fm(font);
        for (const TrackEntry &tr : tracks) {
            const QPointF s = t.map(trackPlane(tr));
            const QString top = QString::number(tr.f[Track::TrackTop]);
            const QString pos = QStringLiteral("%1 - %2")
                                    .arg(qRound(tr.azimuthDeg()) % 360)
                                    .arg(qRound(tr.rangeM() / 1000.0));
            drawShadowText(p, QPointF(s.x() - fm.horizontalAdvance(top) / 2.0,
                                      s.y() - size / 2.0 - 2.0 - fm.descent()),
                           top, st.colors.trackProfile);
            drawShadowText(p, QPointF(s.x() - fm.horizontalAdvance(pos) / 2.0,
                                      s.y() + size / 2.0 + 2.0 + fm.ascent()),
                           pos, st.colors.trackProfile);
        }
    }

    // ---- khung "Theo dõi" bản gọn (analysis-results/05 mục 5), bám phía sau
    // quỹ đạo, lệch sang trái hướng bay để không trùm hết lên vết vừa đi qua.
    for (const TrackEntry &tr : tracks) {
        if (!tr.followed)
            continue;
        QVector<NoteLine> lines;
        lines.append(NoteLine{QStringLiteral("Tốp: %1").arg(tr.f[Track::TrackTop])});
        lines.append(NoteLine{QStringLiteral("%1° - %2km")
                                  .arg(tr.azimuthDeg(), 0, 'f', 1)
                                  .arg(tr.rangeM() / 1000.0, 0, 'f', 1)});
        const quint32 v = tr.f[Track::Velocity];
        lines.append(NoteLine{QStringLiteral("%1m/s (%2km/h)").arg(v).arg(qRound(v * 3.6))});
        lines.append(NoteLine{QStringLiteral("Hướng: %1°").arg(tr.f[Track::Heading] / 100.0, 0, 'f', 1)});
        appendMode(&lines, tr.f[Track::IffReturnedMode]);

        const double rad = qDegreesToRadians(tr.f[Track::Heading] / 100.0);
        const QPointF ahead(std::sin(rad), -std::cos(rad));
        const QPointF left(ahead.y(), -ahead.x());
        const QPointF u = (left - ahead) / std::sqrt(2.0);
        const double gap = size / 2.0 + (st.showTrackProfile ? 10.0 : 5.0);
        const QRectF box = boxToward(t.map(trackPlane(tr)), u, gap, noteSize(lines, font));
        drawNote(p, box, lines, font,
                 tr.type() == Track::TypeVq ? st.colors.track : st.colors.trackMh);
    }
}

void TargetLayer::drawPlots(QPainter &p, const QTransform &t, const QRectF &view, const QFont &font) const
{
    if (m_plots.isEmpty())
        return;
    const Setups &st = Settings::instance().setups();
    const double side = kPlotSizePx * st.plotSizePct / 100.0;

    QPen edge(QColor(0, 0, 0, 140), 1.0);
    edge.setCosmetic(true);
    p.setPen(edge);
    p.setBrush(st.colors.plot);
    for (const PlotMark &m : m_plots) {
        const QPointF s = t.map(m.plane);
        p.drawRect(QRectF(s.x() - side / 2.0, s.y() - side / 2.0, side, side));
    }

    if (!st.showPlotInfo)
        return;
    // Ô thông tin phía dưới bên phải điểm dấu (step-06 mục 7); sát mép phải /
    // mép dưới panel thì lật sang trái / lên trên cho khỏi bị cắt.
    for (const PlotMark &m : m_plots) {
        QVector<NoteLine> lines;
        appendMode(&lines, m.f[Plot::Retmode]);
        lines.append(NoteLine{QStringLiteral("Vị trí: %1° - %2 km")
                                  .arg((m.f[Plot::Azm] % 36000u) / 100.0, 0, 'f', 3)
                                  .arg(m.f[Plot::Range] / 1000.0, 0, 'f', 3)});
        for (const QString &d : IffText::details(m.f[Plot::Retmode], m.f[Plot::Commander],
                                                 m.f[Plot::Flightid], m.f[Plot::Altitude],
                                                 m.f[Plot::Fuellevel]))
            lines.append(NoteLine{d});
        const QPointF s = t.map(m.plane);
        const QSizeF size = noteSize(lines, font);
        const double off = side / 2.0 + 3.0;
        double x = s.x() + off;
        double y = s.y() + off;
        if (x + size.width() > view.right() && s.x() - off - size.width() >= view.left())
            x = s.x() - off - size.width();
        if (y + size.height() > view.bottom() && s.y() - off - size.height() >= view.top())
            y = s.y() - off - size.height();
        drawNote(p, QRectF(QPointF(x, y), size), lines, font, st.colors.plot);
    }
}
