#include "net/ScnTextSession.h"

#include <QTcpSocket>
#include <QTimer>

namespace {

constexpr int kFirstStartMs = 1000;
constexpr int kRepeatStartMs = 5000;
// Dòng lệnh dài nhất hợp lý; quá cỡ này mà chưa thấy xuống dòng thì PC đang gửi
// rác, bỏ bộ đệm để không phình mãi.
constexpr int kMaxLineBytes = 64 * 1024;

} // namespace

ScnTextSession::ScnTextSession(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);

    m_status.listening = true;
    m_status.connected = true;
    m_status.peer = m_socket->peerAddress().toString();
    m_status.scnrpmode = QString::fromLatin1(m_state.scnrpmode);
    m_status.scnrpband = QString::fromLatin1(m_state.scnrpband);
    m_status.scnselrprz = QString::fromLatin1(m_state.scnselrprz);

    m_startTimer = new QTimer(this);
    m_startTimer->setSingleShot(true);
    m_startTimer->setInterval(kFirstStartMs);
    connect(m_startTimer, &QTimer::timeout, this, &ScnTextSession::sendStart);
    m_startTimer->start();

    // Tính cả lúc PC vừa nối mà chưa nói gì: im từ đầu cũng là treo.
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kIdleTimeoutMs);
    connect(m_idleTimer, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        m_socket->abort();
        finish();
    });
    m_idleTimer->start();

    connect(m_socket, &QTcpSocket::readyRead, this, &ScnTextSession::readLines);
    // Đọc trả 0 byte / đầu bên kia đóng: quay lại chờ kết nối. SW1 không nhận ra
    // lúc này (ReadLine trả null) và có thể quay vòng rỗng mãi.
    connect(m_socket, &QTcpSocket::disconnected, this, &ScnTextSession::finish);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ScnTextSession::finish);
}

void ScnTextSession::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_startTimer->stop();
    m_idleTimer->stop();
    emit finished();
}

ScnTextSession::~ScnTextSession()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

bool ScnTextSession::write(const QByteArray &text)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;
    return m_socket->write(text) == text.size();
}

void ScnTextSession::sendStart()
{
    write(ScnText::startBlock());
    m_startTimer->setInterval(kRepeatStartMs);
    m_startTimer->start();
}

void ScnTextSession::readLines()
{
    m_idleTimer->start();
    m_buffer.append(m_socket->readAll());

    // SW1 đọc bằng ReadLine(): \n, \r hay \r\n đều kết thúc một dòng. Dòng rỗng
    // sinh ra giữa \r và \n bị bỏ ở handleLine.
    int start = 0;
    for (int i = 0; i < m_buffer.size(); ++i) {
        const char c = m_buffer.at(i);
        if (c != '\n' && c != '\r')
            continue;
        handleLine(m_buffer.mid(start, i - start));
        start = i + 1;
    }
    m_buffer.remove(0, start);
    if (m_buffer.size() > kMaxLineBytes)
        m_buffer.clear();
}

void ScnTextSession::handleLine(const QByteArray &raw)
{
    const QByteArray line = raw.trimmed();
    if (line.isEmpty())
        return;

    ++m_status.linesIn;
    emit lineReceived(line);
    const ScnText::Reply reply = ScnText::answer(line, &m_state);
    if (!reply.text.isEmpty())
        write(reply.text);
    if (reply.sendStart)
        sendStart();

    if (!reply.known) {
        m_status.unknownLine = QString::fromLatin1(line);
        emit unknownCommand(m_status.unknownLine);
    }
    if (!reply.keepalive.isEmpty())
        m_status.keepalive = QString::fromLatin1(reply.keepalive);
    m_status.scnrpmode = QString::fromLatin1(m_state.scnrpmode);
    m_status.scnrpband = QString::fromLatin1(m_state.scnrpband);
    m_status.scnselrprz = QString::fromLatin1(m_state.scnselrprz);
    emit statusChanged(m_status);
}
