#pragma once

#include "proto/Asterix.h"

#include <QElapsedTimer>
#include <QObject>

struct TrackEntry;

// Dựng các gói ASTERIX MX18 gửi VQ trên dòng SCH-VQ (analysis-results/03):
// quỹ đạo mỗi lần cập nhật, điểm dấu MH, North marker / Sector crossing theo
// góc anten. Lớp này chỉ dựng byte và phát datagram(); gửi qua dòng nào là
// việc của lớp gọi, nhờ vậy kiểm thử được mà không cần mạng.
class VqSender : public QObject
{
    Q_OBJECT
public:
    enum SweepSource { SweepVideoR = 0, SweepVideoI };

    struct Config {
        Asterix::EncodeConfig encode;
        int sweepSource = SweepVideoR;
        bool sendTre = true;
        Asterix::Site site;
    };

    explicit VqSender(QObject *parent = nullptr);

    void setConfig(const Config &config) { m_cfg = config; }
    const Config &config() const { return m_cfg; }

    // endOfTrack: bản tin cuối khi MX18 xoá quỹ đạo (TRE = 1, anh Linh chốt).
    void sendTrack(const TrackEntry &track, bool endOfTrack);
    // plot là mảng Plot::Count trường của gói PLOT.
    void sendPlot(const quint32 *plot);

    // Gọi với mọi tia VIDEO_R / VIDEO_I (azimuth 0..4095); chỉ nguồn khớp
    // vq_sector_source được dùng — SW0 phát xen hai nguồn nên chu kỳ quay
    // trong North marker bị tính lẫn giữa hai anten.
    void sweep(int source, quint32 azimuth4096);
    void resetSweep();

    // Điều kiện của SW0: quỹ đạo sát tâm đài là rác của bộ bám, không gửi.
    static constexpr double kMinTrackRangeM = 300.0;
    // Chu kỳ quay trong North marker kẹp như SW0; lần đầu chưa đo được thì lấy
    // giá trị trên.
    static constexpr double kMinPeriodS = 4.0;
    static constexpr double kMaxPeriodS = 12.0;

signals:
    void datagram(const QByteArray &bytes);

private:
    Config m_cfg;
    QElapsedTimer m_clock;
    int m_lastSector = -1;
    bool m_hasNorth = false;
    qint64 m_northMs = 0;
    int m_northSector = 0;
};
