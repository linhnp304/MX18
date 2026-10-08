#include "track/VqSender.h"

#include "proto/Packets.h"
#include "track/TrackStore.h"

VqSender::VqSender(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

void VqSender::sendTrack(const TrackEntry &t, bool endOfTrack)
{
    if (endOfTrack ? !m_cfg.sendTre : t.rangeM() <= kMinTrackRangeM)
        return;

    Asterix::TargetOut o;
    o.isTrack = true;
    // Quỹ đạo đã có nhận dạng MH gửi kiểu IFF, còn lại là quỹ đạo radar.
    o.iff = (t.type() == Track::TypeVqMh || t.type() == Track::TypeMh);
    o.trackNumber = quint16(t.id() & 0x0fffu);
    o.rangeM = t.rangeM();
    o.azimuthDeg = t.azimuthDeg();
    o.speedMps = t.f[Track::Velocity];
    o.headingDeg = t.f[Track::Heading] / 100.0;
    o.heightM = t.f[Track::IffAltitude] > 0 ? double(t.f[Track::IffAltitude]) : t.heightM;
    o.endOfTrack = endOfTrack;
    o.returnedMode = quint8(t.f[Track::IffReturnedMode]);
    o.commander = (t.f[Track::IffCommander] == 1);
    o.flightId = t.f[Track::IffFlightid];
    o.fuel = quint8(qMin<quint32>(t.f[Track::IffFuellevel], 255));
    // Bản tin cập nhật mang giờ của vị trí: lúc hợp nhất điểm dấu, vị trí còn là
    // của lần X18-VQ trước, gắn giờ hiện tại thì VQ thấy quỹ đạo nhảy lùi. Bản
    // tin cuối (TRE) vẫn lấy giờ hiện tại để không trùng giờ bản tin đã gửi.
    const quint32 tod = (!endOfTrack && t.positionTod) ? t.positionTod : Asterix::timeOfDayNow();
    emit datagram(Asterix::targetReport(m_cfg.encode, tod, o));
}

void VqSender::sendPlot(const quint32 *p)
{
    // Mọi điểm dấu MH đều gửi (như SW0), không xét cự ly.
    Asterix::TargetOut o;
    o.isTrack = false;
    o.iff = true;
    o.rangeM = p[Plot::Range];
    o.azimuthDeg = p[Plot::Azm] / 100.0;
    o.heightM = p[Plot::Altitude];
    o.returnedMode = quint8(p[Plot::Retmode]);
    o.commander = (p[Plot::Commander] == 1);
    o.flightId = p[Plot::Flightid];
    o.fuel = quint8(qMin<quint32>(p[Plot::Fuellevel], 255));
    emit datagram(Asterix::targetReport(m_cfg.encode, Asterix::timeOfDayNow(), o));
}

void VqSender::resetSweep()
{
    m_lastSector = -1;
    m_hasNorth = false;
}

void VqSender::sweep(int source, quint32 azimuth4096)
{
    if (source != m_cfg.sweepSource)
        return;
    const int sector = int((azimuth4096 & 0x0fffu) >> 4);   // 0..255, LSB 1,40625°
    // Tia đầu tiên chỉ ghi nhận vị trí: chưa biết anten có đang quay hay không.
    if (m_lastSector < 0 || sector == m_lastSector) {
        m_lastSector = sector;
        return;
    }
    const int prev = m_lastSector;
    m_lastSector = sector;
    const quint32 tod = Asterix::timeOfDayNow();

    // 32 Sector crossing mỗi vòng (mỗi 11,25°), mang đúng sector lúc đó.
    if ((sector >> 3) != (prev >> 3))
        emit datagram(Asterix::sectorCrossing(m_cfg.encode, tod, quint8(sector)));

    // North marker khi vừa bước vào dải 0 … 22,5°.
    if ((sector >> 4) != (prev >> 4) && (sector >> 4) == 0) {
        const qint64 now = m_clock.elapsed();
        double period = kMaxPeriodS;
        if (m_hasNorth) {
            // Hai lần qua Bắc có thể rơi vào hai sector khác nhau trong dải đầu:
            // quy đổi về đúng một vòng 256 sector rồi chia tỷ lệ.
            int d = (sector - m_northSector + 256) % 256;
            if (d < 128)
                d += 256;
            period = (now - m_northMs) / 1000.0 * 256.0 / d;
        }
        m_hasNorth = true;
        m_northMs = now;
        m_northSector = sector;
        emit datagram(Asterix::northMarker(m_cfg.encode, tod, qBound(kMinPeriodS, period, kMaxPeriodS),
                                           m_cfg.site));
    }
}
