#include "core/DataClock.h"

#include <QElapsedTimer>

namespace DataClock {

namespace {

struct State {
    State() { timer.start(); }

    QElapsedTimer timer;
    // nowMs() lúc chạy thật = liveOffset + timer.elapsed(); về chạy thật sau
    // phát lại thì liveOffset kéo cho đồng hồ nối tiếp, không lùi.
    qint64 liveOffset = 0;

    bool replay = false;
    qint64 replayNow = 0;
    qint64 fileOffset = 0;     // replayNow = fileOffset + ms từ đầu file
    bool needAnchor = true;
    qint64 wallMs = 0;
};

State &state()
{
    static State s;
    return s;
}

} // namespace

qint64 nowMs()
{
    const State &s = state();
    return s.replay ? s.replayNow : s.liveOffset + s.timer.elapsed();
}

QDateTime wallNow()
{
    const State &s = state();
    return s.replay && s.wallMs > 0 ? QDateTime::fromMSecsSinceEpoch(s.wallMs) : QDateTime::currentDateTime();
}

bool replaying()
{
    return state().replay;
}

void beginReplay()
{
    State &s = state();
    s.replayNow = nowMs();
    s.replay = true;
    s.needAnchor = true;
    s.wallMs = 0;
}

void setReplayTime(qint64 fileMs, qint64 wallMsSinceEpoch)
{
    State &s = state();
    if (!s.replay)
        return;
    if (s.needAnchor) {
        s.fileOffset = s.replayNow - fileMs;
        s.needAnchor = false;
    }
    s.replayNow = qMax(s.replayNow, s.fileOffset + fileMs);
    s.wallMs = wallMsSinceEpoch;
}

void jump()
{
    state().needAnchor = true;
}

void endReplay()
{
    State &s = state();
    if (!s.replay)
        return;
    s.replay = false;
    s.liveOffset = s.replayNow - s.timer.elapsed();
}

} // namespace DataClock
