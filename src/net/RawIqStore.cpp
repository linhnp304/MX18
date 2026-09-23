#include "net/RawIqStore.h"

#include <QMutexLocker>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

} // namespace

RawIqStore::RawIqStore()
{
    m_data.sum.assign(RawIq::kAzimuthSteps, kNaN);
    m_data.sub.assign(RawIq::kAzimuthSteps, kNaN);
}

void RawIqStore::feed(const char *raw, int size, bool bigEndian)
{
    if (!m_running.load(std::memory_order_relaxed))
        return;

    RawIq::Header h;
    if (!RawIq::readHeader(raw, size, bigEndian, &h))
        return;

    const int start = m_start.load(std::memory_order_relaxed);
    const int mean = m_mean.load(std::memory_order_relaxed);

    // Phần tính toán nằm ngoài khoá: luồng giao diện chỉ phải chờ đúng lúc
    // chép kết quả, không phải chờ cả vòng lặp 600 word.
    if (RawIq::isBeamType(h.type)) {
        double sum = 0.0;
        double sub = 0.0;
        RawIq::beamMeans(raw, bigEndian, start, mean, &sum, &sub);

        QMutexLocker lock(&m_mutex);
        if (m_data.beamType != h.type) {
            // CS F2 và CS F3 là hai kênh khác nhau: vẽ chồng lên nhau thì
            // đường cánh sóng thành nửa nọ nửa kia.
            std::fill(m_data.sum.begin(), m_data.sum.end(), kNaN);
            std::fill(m_data.sub.begin(), m_data.sub.end(), kNaN);
            m_data.beamType = h.type;
        }
        if (!std::isnan(sum))
            m_data.sum[size_t(h.azm4096)] = sum;
        if (!std::isnan(sub))
            m_data.sub[size_t(h.azm4096)] = sub;
        m_data.beamLast = h.azm4096;
        ++m_data.beamSeq;
        m_data.type = h.type;
        m_data.azm4096 = h.azm4096;
        ++m_data.seq;
        return;
    }

    qint16 iq1[RawIq::kViewPoints];
    qint16 iq2[RawIq::kViewPoints];
    RawIq::extractView(raw, bigEndian, start, iq1, iq2);

    QMutexLocker lock(&m_mutex);
    m_data.iq1.assign(iq1, iq1 + RawIq::kViewPoints);
    m_data.iq2.assign(iq2, iq2 + RawIq::kViewPoints);
    m_data.viewType = h.type;
    m_data.viewStart = qBound(0, start, RawIq::kViewStartMax);
    ++m_data.viewSeq;
    m_data.type = h.type;
    m_data.azm4096 = h.azm4096;
    ++m_data.seq;
}

void RawIqStore::setRunning(bool running)
{
    if (running) {
        QMutexLocker lock(&m_mutex);
        clearLocked();
    }
    m_running.store(running);
}

void RawIqStore::setWindow(int startWord, int meanWords)
{
    m_start.store(startWord);
    m_mean.store(meanWords);
}

void RawIqStore::clearLocked()
{
    // Chỉ số thứ tự không quay về 0 mà tăng lên, để update() thấy "có thay đổi"
    // và chép luôn trạng thái rỗng sang cửa sổ.
    m_data.type = 0;
    m_data.azm4096 = 0;
    ++m_data.seq;

    m_data.viewType = 0;
    m_data.iq1.clear();
    m_data.iq2.clear();
    ++m_data.viewSeq;

    m_data.beamType = 0;
    m_data.beamLast = -1;
    std::fill(m_data.sum.begin(), m_data.sum.end(), kNaN);
    std::fill(m_data.sub.begin(), m_data.sub.end(), kNaN);
    ++m_data.beamSeq;
}

bool RawIqStore::update(Snapshot *out) const
{
    QMutexLocker lock(&m_mutex);
    if (out->seq == m_data.seq)
        return false;
    out->seq = m_data.seq;
    out->type = m_data.type;
    out->azm4096 = m_data.azm4096;

    if (out->viewSeq != m_data.viewSeq) {
        out->viewSeq = m_data.viewSeq;
        out->viewType = m_data.viewType;
        out->viewStart = m_data.viewStart;
        out->iq1 = m_data.iq1;
        out->iq2 = m_data.iq2;
    }
    if (out->beamSeq != m_data.beamSeq) {
        out->beamSeq = m_data.beamSeq;
        out->beamType = m_data.beamType;
        out->beamLast = m_data.beamLast;
        out->sum = m_data.sum;
        out->sub = m_data.sub;
    }
    return true;
}
