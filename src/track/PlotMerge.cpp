#include "track/PlotMerge.h"

#include "core/GeoCalc.h"
#include "proto/Packets.h"
#include "track/TrackStore.h"

#include <cmath>

namespace PlotMerge {

bool Window::contains(double azDeg, double rangeM) const
{
    if (rangeM < rangeStartM || rangeM > rangeStopM)
        return false;
    if (fullCircle)
        return true;
    if (azStartDeg <= azStopDeg)
        return azDeg >= azStartDeg && azDeg <= azStopDeg;
    return azDeg >= azStartDeg || azDeg <= azStopDeg;
}

Window window(double azDeg, double rangeM, double halfAzDeg, double halfRangeM)
{
    Window w;
    w.fullCircle = halfAzDeg >= 180.0;
    w.azStartDeg = azDeg - halfAzDeg;
    w.azStopDeg = azDeg + halfAzDeg;
    if (w.azStartDeg < 0.0)
        w.azStartDeg += 360.0;
    if (w.azStopDeg > 360.0)
        w.azStopDeg -= 360.0;
    w.rangeStartM = qMax(0.0, rangeM - halfRangeM);
    w.rangeStopM = qMin(kMaxRangeM, rangeM + halfRangeM);
    return w;
}

int pick(const QVector<TrackEntry> &tracks, const Window &w, const QPointF &plotPlane)
{
    int best = -1;
    bool bestMerged = false;
    double bestDist = 0.0;
    for (int i = 0; i < tracks.size(); ++i) {
        const TrackEntry &t = tracks.at(i);
        if (t.type() == Track::TypeMh || !w.contains(t.azimuthDeg(), t.rangeM()))
            continue;
        const bool merged = (t.type() == Track::TypeVqMh);
        const QPointF d = LocalProjection::planeFromPolar(t.azimuthDeg(), t.rangeM() / 1000.0) - plotPlane;
        const double dist = std::hypot(d.x(), d.y());
        if (best < 0 || (merged && !bestMerged) || (merged == bestMerged && dist < bestDist)) {
            best = i;
            bestMerged = merged;
            bestDist = dist;
        }
    }
    return best;
}

bool validMode(quint32 retmode)
{
    return retmode >= 1 && retmode <= 9;
}

bool applyIdentity(TrackEntry *t, const quint32 *plot)
{
    const quint32 before[] = {t->f[Track::TrackType], t->f[Track::IffReturnedMode], t->f[Track::IffCommander],
                              t->f[Track::IffFlightid], t->f[Track::IffAltitude], t->f[Track::IffFuellevel]};

    const quint32 mode = plot[Plot::Retmode];
    if (t->type() == Track::TypeVq)
        t->f[Track::TrackType] = Track::TypeVqMh;
    t->f[Track::IffReturnedMode] = mode;
    // Giá trị 0 của điểm dấu nghĩa là "không có" (step-06 gói PLOT), không phải
    // "đã hết": vòng chế độ 1 không được xoá số hiệu lấy từ vòng chế độ 4.
    if (mode == 3 && plot[Plot::Commander] == 1)
        t->f[Track::IffCommander] = 1;
    if (mode == 4 && plot[Plot::Flightid] > 0)
        t->f[Track::IffFlightid] = plot[Plot::Flightid];
    if (mode == 6 && plot[Plot::Altitude] > 0)
        t->f[Track::IffAltitude] = plot[Plot::Altitude];
    if (mode == 6 && plot[Plot::Fuellevel] > 0)
        t->f[Track::IffFuellevel] = plot[Plot::Fuellevel];

    const quint32 after[] = {t->f[Track::TrackType], t->f[Track::IffReturnedMode], t->f[Track::IffCommander],
                             t->f[Track::IffFlightid], t->f[Track::IffAltitude], t->f[Track::IffFuellevel]};
    for (int i = 0; i < int(sizeof(before) / sizeof(before[0])); ++i) {
        if (before[i] != after[i])
            return true;
    }
    return false;
}

} // namespace PlotMerge
