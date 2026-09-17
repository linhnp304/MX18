#include "net/PingService.h"

#include <QProcess>
#include <QThread>
#include <QTimer>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

constexpr int kPeriodMs = 3000;
constexpr int kNodesPerThread = 3;

bool pingOnce(const QString &address)
{
#if defined(Q_OS_WIN)
    const QStringList args{QStringLiteral("-n"), QStringLiteral("1"),
                           QStringLiteral("-w"), QStringLiteral("1000"), address};
#else
    const QStringList args{QStringLiteral("-c"), QStringLiteral("1"),
                           QStringLiteral("-W"), QStringLiteral("1"), address};
#endif
    // Không dùng QProcess::execute: nó cho tiến trình ping ghi thẳng ra stdout
    // của phần mềm, làm bẩn log chạy.
    QProcess proc;
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    proc.start(QStringLiteral("ping"), args);
    if (!proc.waitForStarted(2000))
        return false;
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(500);
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

} // namespace

PingWorker::PingWorker(const QVector<int> &indices, const QVector<QString> &addresses, int periodMs)
    : m_indices(indices)
    , m_addresses(addresses)
    , m_periodMs(periodMs)
{
}

void PingWorker::begin()
{
    m_timer = new QTimer(this);
    m_timer->setInterval(m_periodMs);
    connect(m_timer, &QTimer::timeout, this, &PingWorker::tick);
    m_timer->start();
    tick(); // kiểm tra ngay lần đầu, không chờ hết chu kỳ
}

void PingWorker::finish()
{
    if (m_timer)
        m_timer->stop();
}

void PingWorker::tick()
{
    for (int i = 0; i < m_indices.size(); ++i)
        emit nodeState(m_indices.at(i), pingOnce(m_addresses.at(i)));
}

PingService::PingService(QObject *parent)
    : QObject(parent)
{
}

PingService::~PingService()
{
    stop();
}

void PingService::start(const QVector<NetNode> &nodes)
{
    stop();
    m_nodes = nodes;
    m_alive.fill(false, nodes.size());
    m_known.fill(false, nodes.size());
    m_lastLevel = -1;

    for (int base = 0; base < nodes.size(); base += kNodesPerThread) {
        QVector<int> idx;
        QVector<QString> addr;
        for (int k = 0; k < kNodesPerThread && base + k < nodes.size(); ++k) {
            idx.append(base + k);
            addr.append(nodes.at(base + k).address);
        }

        auto *thread = new QThread(this);
        auto *worker = new PingWorker(idx, addr, kPeriodMs);
        worker->moveToThread(thread);
        connect(thread, &QThread::started, worker, &PingWorker::begin);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &PingWorker::nodeState, this, &PingService::onNodeState,
                Qt::QueuedConnection);
        m_threads.append(thread);
        thread->start();
    }
}

void PingService::stop()
{
    for (QThread *t : std::as_const(m_threads)) {
        t->quit();
        // Một lần ping có thể kéo dài tới ~1 giây; chờ đủ lâu để luồng thoát sạch.
        if (!t->wait(3000))
            t->terminate();
        t->deleteLater();
    }
    m_threads.clear();
}

bool PingService::isAlive(int index) const
{
    return (index >= 0 && index < m_alive.size()) ? m_alive.at(index) : false;
}

int PingService::overallLevel() const
{
    int level = 0;
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes.at(i).kind == 0)
            continue; // nút không cảnh báo không ảnh hưởng màu biểu tượng
        const bool down = m_known.at(i) ? !m_alive.at(i) : true;
        if (!down)
            continue;
        if (m_nodes.at(i).kind == 2)
            return 2;
        level = qMax(level, 1);
    }
    return level;
}

void PingService::onNodeState(int index, bool alive)
{
    if (index < 0 || index >= m_alive.size())
        return;
    const bool changed = !m_known.at(index) || m_alive.at(index) != alive;
    m_alive[index] = alive;
    m_known[index] = true;
    if (changed)
        emit nodeStateChanged(index, alive);

    const int level = overallLevel();
    if (level != m_lastLevel) {
        m_lastLevel = level;
        emit overallLevelChanged(level);
    }
}
