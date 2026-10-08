#include "track/TrackStore.h"

#include "proto/Asterix.h"
#include "track/PlotMerge.h"

#include <QTimer>
#include <QtMath>

#include <cmath>
#include <cstring>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

quint32 centiDegrees(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0)
        d += 360.0;
    return quint32(std::lround(d * 100.0)) % 36000u;
}

// Thiếu I048/200 thì lấy vận tốc theo hai lần cập nhật liên tiếp; dưới ngưỡng
// này hai vị trí quá gần về thời gian, sai số đo cự ly làm vận tốc vô nghĩa.
constexpr qint64 kMinVelocityIntervalMs = 500;

} // namespace

TrackStore::TrackStore(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
    // Một giây một lần là đủ mịn cho ngưỡng xoá tính bằng chục giây.
    m_expireTimer = new QTimer(this);
    m_expireTimer->setInterval(1000);
    connect(m_expireTimer, &QTimer::timeout, this, &TrackStore::expire);
    m_expireTimer->start();
}

void TrackStore::setCenter(double lat, double lon)
{
    m_proj.setCenter(lat, lon);
    // lat/lng của TRACK tính từ phương vị - cự ly và tâm đài: đổi tâm đài thì
    // tính lại cho khớp với chỗ vẽ.
    for (TrackEntry &t : m_tracks)
        setLatLng(&t);
}

void TrackStore::setLatLng(TrackEntry *t) const
{
    const GeoPoint g = m_proj.toGeo(LocalProjection::planeFromPolar(t->azimuthDeg(), t->rangeM() / 1000.0));
    t->f[Track::Lat] = Track::rawLatLng(g.lat);
    t->f[Track::Lng] = Track::rawLatLng(g.lon);
}

int TrackStore::indexOf(quint32 id) const
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).id() == id)
            return i;
    }
    return -1;
}

const TrackEntry *TrackStore::find(quint32 id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &m_tracks.at(i);
}

void TrackStore::applyVq(const Asterix::Cat048 &r)
{
    if (!r.trackNumber)
        return;
    const quint32 id = *r.trackNumber;

    if (r.tre) {
        // Bản tin cuối: xoá ngay, không cập nhật vị trí cuối (analysis-results/05 mục 1).
        remove(id, RemovedEndOfTrack);
        return;
    }

    // Vị trí Descartes là kết quả bám của P18M nên ưu tiên hơn vị trí đo cực.
    bool hasPos = false;
    double azDeg = 0.0;
    double rangeM = 0.0;
    if (r.x && r.y) {
        azDeg = std::atan2(*r.x, *r.y) * 180.0 / M_PI;
        rangeM = std::hypot(*r.x, *r.y);
        hasPos = true;
    } else if (r.rangeM && r.azimuthDeg) {
        azDeg = *r.azimuthDeg;
        rangeM = *r.rangeM;
        hasPos = true;
    }

    const qint64 now = m_clock.elapsed();
    int i = indexOf(id);
    const bool added = (i < 0);
    if (added) {
        // Chưa biết vị trí thì chưa đặt được quỹ đạo lên màn hình: chờ bản ghi sau.
        if (!hasPos)
            return;
        TrackEntry t;
        std::memcpy(t.f, Track::defaults(), sizeof(t.f));
        t.f[Track::TrackType] = Track::TypeVq;
        t.f[Track::TrackStatus] = Track::StatusTracking;
        t.f[Track::TrackId] = id;
        t.f[Track::TrackTop] = id;
        m_tracks.append(t);
        i = int(m_tracks.size()) - 1;
    }
    TrackEntry &t = m_tracks[i];

    if (hasPos) {
        const double oldAz = t.azimuthDeg();
        const double oldRange = t.rangeM();
        if (!added)
            t.pushHistory();
        t.f[Track::Azm] = centiDegrees(azDeg);
        t.f[Track::Range] = quint32(std::lround(qMax(0.0, rangeM)));
        t.positionTod = Asterix::timeOfDayNow();
        setLatLng(&t);

        if (!r.speed && !added && now - t.updatedMs >= kMinVelocityIntervalMs) {
            const QPointF a = LocalProjection::planeFromPolar(oldAz, oldRange);
            const QPointF b = LocalProjection::planeFromPolar(azDeg, rangeM);
            const double dx = b.x() - a.x();
            const double dy = b.y() - a.y();
            const double dt = (now - t.updatedMs) / 1000.0;
            t.f[Track::Velocity] = quint32(std::lround(std::hypot(dx, dy) / dt));
            t.f[Track::Heading] = centiDegrees(std::atan2(dx, dy) * 180.0 / M_PI);
        }
    }
    if (r.speed && r.heading) {
        t.f[Track::Velocity] = quint32(std::lround(*r.speed));
        t.f[Track::Heading] = centiDegrees(*r.heading);
    }
    if (r.heightM)
        t.heightM = *r.heightM;
    t.updatedMs = now;

    emit trackUpdated(t, added);
}

bool TrackStore::mergePlot(const quint32 *plot, double halfAzDeg, double halfRangeKm, quint32 *mergedId)
{
    if (!PlotMerge::validMode(plot[Plot::Retmode]))
        return false;
    const double azDeg = (plot[Plot::Azm] % 36000u) / 100.0;
    const double rangeM = double(plot[Plot::Range]);
    const PlotMerge::Window w = PlotMerge::window(azDeg, rangeM, halfAzDeg, halfRangeKm * 1000.0);
    const int i = PlotMerge::pick(m_tracks, w, LocalProjection::planeFromPolar(azDeg, rangeM / 1000.0));
    if (i < 0)
        return false;
    TrackEntry &t = m_tracks[i];
    if (mergedId)
        *mergedId = t.id();
    // Vị trí giữ nguyên của X18-VQ: điểm dấu chỉ mang nhận dạng vào, và hạn xoá
    // theo giờ vẫn tính theo lần cập nhật từ nguồn quỹ đạo.
    if (PlotMerge::applyIdentity(&t, plot))
        emit trackUpdated(t, false);
    return true;
}

void TrackStore::applyMhPlot(const quint32 *plot)
{
    MhTracker::Events ev;
    if (m_mh.plot(m_tracks, plot, m_clock.elapsed(), &ev))
        emitMhEvents(ev);
}

void TrackStore::mhSweep(double azDeg)
{
    MhTracker::Events ev;
    m_mh.sweep(m_tracks, azDeg, m_clock.elapsed(), &ev);
    if (!ev.added.isEmpty() || !ev.updated.isEmpty() || !ev.expired.isEmpty())
        emitMhEvents(ev);
}

void TrackStore::emitMhEvents(const MhTracker::Events &ev)
{
    // Bộ bám không biết tâm đài: lat/lng tính ở đây, ngay trước khi báo đi.
    const auto announce = [this](quint32 id, bool added) {
        const int i = indexOf(id);
        if (i < 0)
            return;
        setLatLng(&m_tracks[i]);
        emit trackUpdated(m_tracks.at(i), added);
    };
    for (quint32 id : ev.added)
        announce(id, true);
    for (quint32 id : ev.updated)
        announce(id, false);
    for (quint32 id : ev.expired)
        remove(id, RemovedExtrapolated);
}

int TrackStore::removeMhTracks(RemoveReason reason)
{
    m_mh.clearPending();
    QVector<quint32> ids;
    for (const TrackEntry &t : std::as_const(m_tracks)) {
        if (t.type() == Track::TypeMh)
            ids.append(t.id());
    }
    for (quint32 id : std::as_const(ids))
        remove(id, reason);
    return int(ids.size());
}

bool TrackStore::remove(quint32 id, RemoveReason reason)
{
    const int i = indexOf(id);
    if (i < 0)
        return false;
    TrackEntry t = m_tracks.takeAt(i);
    t.f[Track::TrackStatus] = Track::StatusDeleted;
    emit trackRemoved(t, reason);
    return true;
}

void TrackStore::removeAll(RemoveReason reason)
{
    // Lấy ra hết trước rồi mới báo: lớp nhận tín hiệu có thể đọc lại danh sách.
    QVector<TrackEntry> gone;
    gone.swap(m_tracks);
    if (reason == RemovedDisconnected)
        m_mh.reset();
    else
        m_mh.clearPending();
    for (TrackEntry &t : gone) {
        t.f[Track::TrackStatus] = Track::StatusDeleted;
        emit trackRemoved(t, reason);
    }
}

bool TrackStore::setFollowed(quint32 id, bool followed)
{
    const int i = indexOf(id);
    if (i < 0 || m_tracks[i].followed == followed)
        return false;
    m_tracks[i].followed = followed;
    emit trackChanged(id);
    return true;
}

bool TrackStore::clearIdentity(quint32 id)
{
    const int i = indexOf(id);
    if (i < 0 || m_tracks[i].type() != Track::TypeVqMh)
        return false;
    TrackEntry &t = m_tracks[i];
    t.f[Track::TrackType] = Track::TypeVq;
    t.f[Track::IffReturnedMode] = 0;
    t.f[Track::IffCommander] = 0;
    t.f[Track::IffFlightid] = 0;
    t.f[Track::IffAltitude] = 0;
    t.f[Track::IffFuellevel] = 0;
    emit trackChanged(id);
    return true;
}

void TrackStore::expire()
{
    const qint64 now = m_clock.elapsed();
    QVector<quint32> stale;
    for (const TrackEntry &t : std::as_const(m_tracks)) {
        if (now - t.updatedMs > m_dropMs)
            stale.append(t.id());
    }
    for (quint32 id : std::as_const(stale))
        remove(id, RemovedTimeout);
}
