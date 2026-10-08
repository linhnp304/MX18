#include "track/MhTracker.h"

#include "core/GeoCalc.h"
#include "proto/Asterix.h"
#include "track/PlotMerge.h"

#include <QtMath>

#include <cmath>
#include <cstring>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

double wrap360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0)
        d += 360.0;
    return d;
}

double wrap180(double deg)
{
    const double d = wrap360(deg);
    return d > 180.0 ? d - 360.0 : d;
}

double azimuthOf(const QPointF &p)
{
    return wrap360(std::atan2(p.x(), p.y()) * 180.0 / M_PI);
}

double rangeKmOf(const QPointF &p)
{
    return std::hypot(p.x(), p.y());
}

double distKm(const QPointF &a, const QPointF &b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

quint32 centiDegrees(double deg)
{
    return quint32(std::lround(wrap360(deg) * 100.0)) % 36000u;
}

// Đường quét đi thuận chiều từ prev đến cur có vượt qua angle không. Bước quá
// nửa vòng là nhảy lùi hoặc mất nhiều tia: không kết luận được gì, để phần
// kiểm tra theo thời gian lo.
bool crossed(double prevDeg, double curDeg, double angleDeg)
{
    const double step = wrap360(curDeg - prevDeg);
    if (step <= 0.0 || step >= 180.0)
        return false;
    const double toAngle = wrap360(angleDeg - prevDeg);
    return toAngle > 0.0 && toAngle <= step;
}

// Chu kỳ quét đo được nằm ngoài dải này là anten vừa dừng rồi quay lại: bỏ.
constexpr double kMinPeriodS = 1.0;
constexpr double kMaxPeriodS = 60.0;

} // namespace

MhTracker::MhTracker()
{
    m_periodS = m_params.scanPeriodS;
    m_nextNumber = m_params.trackIdStart;
}

void MhTracker::setParams(const Params &params)
{
    const bool startChanged = (params.trackIdStart != m_params.trackIdStart);
    m_params = params;
    if (!m_periodMeasured)
        m_periodS = params.scanPeriodS;
    if (startChanged || m_nextNumber < params.trackIdStart || m_nextNumber > 4095)
        m_nextNumber = params.trackIdStart;
}

void MhTracker::reset()
{
    m_pending.clear();
    m_hasSweep = false;
    m_northMs = -1;
}

double MhTracker::scanDt(double periodS, double azFromDeg, double azToDeg)
{
    return periodS * (1.0 + wrap180(azToDeg - azFromDeg) / 360.0);
}

void MhTracker::sweep(QVector<TrackEntry> &tracks, double azDeg, qint64 nowMs, Events *ev)
{
    const double prev = m_hasSweep ? m_prevAzDeg : azDeg;
    if (m_hasSweep && crossed(prev, azDeg, 0.0)) {
        if (m_northMs >= 0) {
            const double p = (nowMs - m_northMs) / 1000.0;
            if (p >= kMinPeriodS && p <= kMaxPeriodS) {
                m_periodS = p;
                m_periodMeasured = true;
            }
        }
        m_northMs = nowMs;
    }
    m_hasSweep = true;
    m_prevAzDeg = azDeg;

    purgePending(nowMs);

    const double halfScanMs = m_periodS * 500.0;
    const double lateMs = m_periodS * 1500.0;
    for (TrackEntry &t : tracks) {
        if (t.type() != Track::TypeMh)
            continue;
        const double age = double(nowMs - t.mh.windowMs);
        // Cửa sổ vừa mở ở vòng này (ngay sau điểm dấu) thì đường quét còn ở
        // sát đó: phải chờ sang vòng sau mới tính là vượt qua.
        if (age < halfScanMs)
            continue;
        // Quá 1,5 vòng mà chưa thấy vượt (mất tia, anten đứng) cũng coi là hết.
        if (!crossed(prev, azDeg, t.mh.window.azStopDeg + kCloseMarginDeg) && age < lateMs)
            continue;
        if (t.mh.misses >= m_params.extrapolateScans) {
            ev->expired.append(t.id());
            continue;
        }
        extrapolate(&t, nowMs);
        ev->updated.append(t.id());
    }
}

bool MhTracker::plot(QVector<TrackEntry> &tracks, const quint32 *plot, qint64 nowMs, Events *ev)
{
    if (!PlotMerge::validMode(plot[Plot::Retmode]))
        return false;
    const double azDeg = (plot[Plot::Azm] % 36000u) / 100.0;
    const double rangeM = double(plot[Plot::Range]);
    const QPointF z = LocalProjection::planeFromPolar(azDeg, rangeM / 1000.0);
    purgePending(nowMs);

    // Quỹ đạo đang bám có cửa sổ chứa điểm dấu: gần tâm dự đoán nhất được nhận.
    int best = -1;
    double bestDist = 0.0;
    for (int i = 0; i < tracks.size(); ++i) {
        const TrackEntry &t = tracks.at(i);
        if (t.type() != Track::TypeMh || !gateOk(t, z, azDeg, rangeM, nowMs))
            continue;
        const double d = distKm(t.mh.predKm, z);
        if (best < 0 || d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    if (best >= 0) {
        absorb(&tracks[best], z, azDeg, plot, nowMs);
        ev->updated.append(tracks.at(best).id());
        return true;
    }

    // Điểm dấu sát một quỹ đạo vừa nhận điểm dấu ở vòng này là chùm bị tách:
    // để nó mở chuỗi chờ thì vòng sau chuỗi đó nối với điểm dấu tách tiếp theo
    // và sinh ra quỹ đạo trùng.
    const double halfScanMs = m_periodS * 500.0;
    for (const TrackEntry &t : std::as_const(tracks)) {
        if (t.type() != Track::TypeMh || double(nowMs - t.mh.windowMs) >= halfScanMs)
            continue;
        const PlotMerge::Window near = PlotMerge::window(t.azimuthDeg(), t.rangeM(), m_params.windowAzimuthDeg,
                                                         m_params.windowRangeKm * 1000.0);
        if (near.contains(azDeg, rangeM))
            return false;
    }

    // Chuỗi chờ khởi tạo: chuỗi dài hơn được ưu tiên, rồi đến chuỗi gần hơn.
    int bestHits = 0;
    for (int i = 0; i < m_pending.size(); ++i) {
        const TrackEntry &p = m_pending.at(i);
        if (!gateOk(p, z, azDeg, rangeM, nowMs))
            continue;
        const double d = distKm(p.mh.predKm, z);
        if (best < 0 || p.mh.hits > bestHits || (p.mh.hits == bestHits && d < bestDist)) {
            best = i;
            bestHits = p.mh.hits;
            bestDist = d;
        }
    }
    TrackEntry fresh;
    if (best >= 0) {
        absorb(&m_pending[best], z, azDeg, plot, nowMs);
        if (m_pending.at(best).mh.hits < m_params.initScans)
            return true;
        fresh = m_pending.takeAt(best);
    } else {
        std::memcpy(fresh.f, Track::defaults(), sizeof(fresh.f));
        fresh.f[Track::TrackType] = Track::TypeMh;
        absorb(&fresh, z, azDeg, plot, nowMs);
        if (m_params.initScans > 1) {
            m_pending.append(fresh);
            return true;
        }
    }

    // Đủ số vòng liên tiếp: thành quỹ đạo track_type = 3, mang theo nhận dạng
    // và vết của các điểm dấu đã khởi tạo nó.
    const quint32 number = allocateNumber(tracks);
    fresh.f[Track::TrackId] = kIdBase + number;
    fresh.f[Track::TrackTop] = number;
    tracks.append(fresh);
    ev->added.append(fresh.id());
    return true;
}

bool MhTracker::gateOk(const TrackEntry &t, const QPointF &zKm, double azDeg, double rangeM, qint64 nowMs) const
{
    // Một vòng quét chỉ nhận một điểm dấu: điểm dấu thứ hai ngay sau (chùm bị
    // tách, hai mục tiêu sát nhau) không được kéo quỹ đạo đi lần nữa.
    if (double(nowMs - t.mh.windowMs) < m_periodS * 500.0)
        return false;
    if (t.mh.hits <= 1) {
        // Mới một điểm dấu thì chưa có hướng bay để dự đoán: nhận mọi điểm dấu
        // vòng sau có vận tốc suy ra nằm trong dải cho phép.
        const double dt = scanDt(m_periodS, azimuthOf(t.mh.posKm), azDeg);
        const double speed = distKm(zKm, t.mh.posKm) * 1000.0 / dt;
        return speed >= m_params.speedMinMps && speed <= m_params.speedMaxMps;
    }
    return t.mh.window.contains(azDeg, rangeM);
}

void MhTracker::absorb(TrackEntry *t, const QPointF &zKm, double azDeg, const quint32 *plot, qint64 nowMs)
{
    MhState &s = t->mh;
    if (s.hits > 0) {
        t->pushHistory();
        const double dt = scanDt(m_periodS, azimuthOf(s.posKm), azDeg);
        if (s.hits == 1) {
            s.velKmS = (zKm - s.posKm) / dt;
            s.posKm = zKm;
        } else {
            const QPointF pred = s.posKm + s.velKmS * dt;
            const QPointF residual = zKm - pred;
            s.posKm = pred + residual * kAlpha;
            // Sau vài vòng ngoại suy, độ lệch là sai số vận tốc dồn qua cả quãng
            // dự đoán: chia cho một vòng thì hướng bay bị sửa quá tay.
            s.velKmS += residual * (kBeta / (dt * (s.misses + 1)));
        }
        const double speedMps = std::hypot(s.velKmS.x(), s.velKmS.y()) * 1000.0;
        if (speedMps > m_params.speedMaxMps)
            s.velKmS *= m_params.speedMaxMps / speedMps;
    } else {
        s.posKm = zKm;
    }
    ++s.hits;
    s.misses = 0;
    // Loại TypeMh giữ nguyên; nhận dạng đã có được giữ suốt đời như mục 8.
    PlotMerge::applyIdentity(t, plot);
    t->f[Track::TrackStatus] = Track::StatusTracking;
    t->updatedMs = nowMs;
    t->positionTod = Asterix::timeOfDayNow();
    writeFields(t);
    openWindow(t, nowMs);
}

void MhTracker::extrapolate(TrackEntry *t, qint64 nowMs)
{
    MhState &s = t->mh;
    t->pushHistory();
    s.posKm = s.predKm;
    ++s.misses;
    t->f[Track::TrackStatus] = Track::StatusExtrapolated;
    t->updatedMs = nowMs;
    t->positionTod = Asterix::timeOfDayNow();
    writeFields(t);
    openWindow(t, nowMs);
}

void MhTracker::openWindow(TrackEntry *t, qint64 nowMs) const
{
    MhState &s = t->mh;
    // Thời gian tới lần chùm tia gặp lại mục tiêu phụ thuộc chỗ mục tiêu sẽ
    // tới: ước lượng một vòng tròn trước, rồi sửa theo phương vị dự đoán.
    const QPointF rough = s.posKm + s.velKmS * m_periodS;
    const double dt = scanDt(m_periodS, azimuthOf(s.posKm), azimuthOf(rough));
    s.predKm = s.posKm + s.velKmS * dt;
    s.window = PlotMerge::window(azimuthOf(s.predKm), rangeKmOf(s.predKm) * 1000.0,
                                 m_params.windowAzimuthDeg, m_params.windowRangeKm * 1000.0);
    s.windowMs = nowMs;
    t->f[Track::WindowAzm1] = centiDegrees(s.window.azStartDeg);
    t->f[Track::WindowAzm2] = centiDegrees(s.window.azStopDeg);
    t->f[Track::WindowRange1] = quint32(std::lround(s.window.rangeStartM));
    t->f[Track::WindowRange2] = quint32(std::lround(s.window.rangeStopM));
}

void MhTracker::writeFields(TrackEntry *t) const
{
    const MhState &s = t->mh;
    t->f[Track::Azm] = centiDegrees(azimuthOf(s.posKm));
    t->f[Track::Range] = quint32(std::lround(rangeKmOf(s.posKm) * 1000.0));
    const double speedMps = std::hypot(s.velKmS.x(), s.velKmS.y()) * 1000.0;
    t->f[Track::Velocity] = quint32(std::lround(speedMps));
    if (speedMps > 0.0)
        t->f[Track::Heading] = centiDegrees(std::atan2(s.velKmS.x(), s.velKmS.y()) * 180.0 / M_PI);
}

void MhTracker::purgePending(qint64 nowMs)
{
    // Quá 1,5 vòng không có điểm dấu tiếp theo: không còn "vòng liên tiếp".
    const double lateMs = m_periodS * 1500.0;
    for (int i = int(m_pending.size()) - 1; i >= 0; --i) {
        if (double(nowMs - m_pending.at(i).mh.windowMs) > lateMs)
            m_pending.removeAt(i);
    }
}

quint32 MhTracker::allocateNumber(const QVector<TrackEntry> &tracks)
{
    // Quay vòng trong [trackIdStart, 4095], bỏ qua số đang có trong danh sách
    // (kể cả Track Number của P18M), để VQ không nhận hai quỹ đạo cùng số.
    const int start = m_params.trackIdStart;
    for (int k = start; k <= 4095; ++k) {
        const quint32 n = quint32(m_nextNumber);
        m_nextNumber = (m_nextNumber >= 4095) ? start : m_nextNumber + 1;
        bool used = false;
        for (const TrackEntry &t : tracks) {
            if ((t.id() & 0x0fffu) == n) {
                used = true;
                break;
            }
        }
        if (!used)
            return n;
    }
    // Hơn nghìn quỹ đạo cùng lúc: chịu trùng số còn hơn bỏ mục tiêu.
    const quint32 n = quint32(m_nextNumber);
    m_nextNumber = (m_nextNumber >= 4095) ? start : m_nextNumber + 1;
    return n;
}
