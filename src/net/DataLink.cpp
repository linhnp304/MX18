#include "net/DataLink.h"

#include "net/ScnTextSession.h"
#include "proto/Dataframe.h"
#include "record/Recorder.h"

#include <QNetworkDatagram>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

// Thời gian thử nối lại khi TCP Client mất kết nối.
constexpr int kRetryMs = 3000;

// Bộ đệm nhận của hệ điều hành cho cổng gói thô: ~200 gói RAW_IQ, đủ đỡ một
// nhịp luồng nhận bị hệ điều hành cho nghỉ lâu. Linux tự kẹp về rmem_max.
constexpr int kRawRecvBuffer = 4 * 1024 * 1024;
// Datagram UDP dài nhất có thể có; đọc thẳng vào bộ đệm cỡ này nên không bao
// giờ phải cấp phát lại.
constexpr int kMaxDatagram = 65536;

QString describe(const LinkEntry &e)
{
    static const char *const kDir[] = {"Recv", "Send", "Send/Recv"};
    const QString proto = (e.protocol == LinkEntry::Tcp)
        ? QStringLiteral("TCP %1").arg(e.type == LinkEntry::ServerOrUnicast
                                           ? QStringLiteral("Server") : QStringLiteral("Client"))
        : QStringLiteral("UDP %1").arg(e.type == LinkEntry::ServerOrUnicast
                                           ? QStringLiteral("Unicast") : QStringLiteral("Broadcast"));
    return QStringLiteral("%1 (%2, %3, %4:%5 → %6:%7)")
        .arg(e.category, QString::fromLatin1(kDir[qBound(0, e.direction, 2)]), proto,
             e.localIp, QString::number(e.localPort), e.remoteIp, QString::number(e.remotePort));
}

} // namespace

// ------------------------------------------------------------------ worker

LinkWorker::LinkWorker(const LinkEntry &entry, std::shared_ptr<RawSink> rawSink)
    : m_entry(entry)
    , m_bigEndian(entry.bigEndian)
    , m_format(entry.format)
    , m_rawSink(entry.format == LinkEntry::RawIq ? std::move(rawSink) : nullptr)
{
    m_remoteAddr = QHostAddress(m_entry.remoteIp);
    m_localAddr = QHostAddress(m_entry.localIp);
}

LinkWorker::~LinkWorker() = default;

void LinkWorker::setRecordSink(std::shared_ptr<RecordSink> sink, int stream)
{
    m_record = std::move(sink);
    m_recordStream = stream;
}

void LinkWorker::record(const char *data, int size)
{
    if (m_record && m_record->isActive())
        m_record->push(m_recordStream, data, size);
}

void LinkWorker::begin()
{
    if (m_format == LinkEntry::RawIq && !m_rawSink) {
        emit message(QStringLiteral("%1: định dạng raw_iq chỉ dùng được cho dòng Data-RAW, "
                                    "gói nhận được sẽ bị bỏ.").arg(m_entry.category), true);
    }
    if (m_format == LinkEntry::ScnText
        && (m_entry.protocol != LinkEntry::Tcp || m_entry.type != LinkEntry::ServerOrUnicast)) {
        // MX18 đóng vai thiết bị SCN nên luôn là bên chờ PC kết nối vào.
        emit message(QStringLiteral("%1: định dạng scn_text chỉ chạy ở TCP Server, dòng này "
                                    "không mở.").arg(m_entry.category), true);
        return;
    }
    if (m_entry.protocol == LinkEntry::Tcp) {
        if (m_entry.type == LinkEntry::ServerOrUnicast)
            openTcpServer();
        else
            openTcpClient();
    } else {
        openUdp();
    }
}

void LinkWorker::finish()
{
    if (m_scn) {
        delete m_scn;
        m_scn = nullptr;
    }
    if (m_retry) {
        m_retry->stop();
        delete m_retry;
        m_retry = nullptr;
    }
    if (m_udp) {
        m_udp->close();
        delete m_udp;
        m_udp = nullptr;
    }
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    const QList<QTcpSocket *> sockets = m_tcpBuffers.keys();
    m_tcpBuffers.clear();
    for (QTcpSocket *s : sockets) {
        // Ngắt tín hiệu trước khi abort(): nếu không, disconnected() sẽ gọi
        // dropTcpSocket() và socket bị huỷ hai lần.
        s->disconnect(this);
        s->abort();
        delete s;
    }
    m_client = nullptr;
}

void LinkWorker::openUdp()
{
    m_udp = new QUdpSocket(this);

    if (m_entry.direction == LinkEntry::Send) {
        // Chỉ gửi: LocalPort = 0 nghĩa là để hệ điều hành tự chọn cổng nguồn.
        // Có cổng cố định (SCH-VQ gửi từ 10770) thì bên nhận có thể lọc theo
        // cổng nguồn, nên máy chưa gắn đúng địa chỉ vẫn giữ cổng trên mọi card.
        if (m_entry.localPort != 0 && !m_udp->bind(m_localAddr, m_entry.localPort)) {
            const QString first = m_udp->errorString();
            if (m_localAddr == QHostAddress(QHostAddress::AnyIPv4)
                || !m_udp->bind(QHostAddress(QHostAddress::AnyIPv4), m_entry.localPort)) {
                emit message(QStringLiteral("Không mở được cổng gửi %1: %2")
                                 .arg(describe(m_entry), first), true);
            } else {
                emit message(QStringLiteral("%1: máy không có địa chỉ %2, gửi từ cổng %3 trên mọi card mạng.")
                                 .arg(m_entry.category, m_entry.localIp).arg(m_entry.localPort), false);
            }
        }
        // SW1 gửi byte "0" cho PC ngay khi mở luồng UDP; giữ lại vì vô hại và PC
        // có thể dựa vào nó để biết SCN đã sẵn sàng.
        if (m_format == LinkEntry::ScnCf)
            writeOut(ScnCf::helloBytes());
        return;
    }

    // Gói quảng bá không đến được socket đã gắn vào một địa chỉ đơn hướng, nên
    // dòng Broadcast phải bind vào 0.0.0.0 rồi lọc người gửi bằng RemoteIP.
    const bool broadcast = (m_entry.type == LinkEntry::ClientOrBroadcast);
    QHostAddress bindAddr = broadcast ? QHostAddress(QHostAddress::AnyIPv4) : m_localAddr;
    if (bindAddr.isNull())
        bindAddr = QHostAddress(QHostAddress::AnyIPv4);

    const QUdpSocket::BindMode mode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
    if (!m_udp->bind(bindAddr, m_entry.localPort, mode)) {
        // Máy trắc thủ chưa gắn card đúng địa chỉ là chuyện thường gặp lúc lắp
        // đặt: thử lại trên mọi giao diện để vẫn nhận được dữ liệu.
        const QString first = m_udp->errorString();
        if (bindAddr == QHostAddress(QHostAddress::AnyIPv4)
            || !m_udp->bind(QHostAddress(QHostAddress::AnyIPv4), m_entry.localPort, mode)) {
            emit message(QStringLiteral("Không mở được cổng nhận %1: %2")
                             .arg(describe(m_entry), first), true);
            return;
        }
        emit message(QStringLiteral("%1: máy không có địa chỉ %2, đang nhận trên mọi card mạng.")
                         .arg(m_entry.category, m_entry.localIp), false);
    }

    if (m_rawSink) {
        m_udp->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, kRawRecvBuffer);
        m_rawBuffer.resize(kMaxDatagram);
    }
    connect(m_udp, &QUdpSocket::readyRead, this, &LinkWorker::readUdp);
}

void LinkWorker::openTcpServer()
{
    m_server = new QTcpServer(this);
    QHostAddress listenAddr = m_localAddr;
    if (listenAddr.isNull())
        listenAddr = QHostAddress(QHostAddress::AnyIPv4);

    if (!m_server->listen(listenAddr, m_entry.localPort)
        && !m_server->listen(QHostAddress(QHostAddress::AnyIPv4), m_entry.localPort)) {
        emit message(QStringLiteral("Không mở được cổng %1: %2")
                         .arg(describe(m_entry), m_server->errorString()), true);
        return;
    }
    if (m_format == LinkEntry::ScnText) {
        connect(m_server, &QTcpServer::newConnection, this, &LinkWorker::acceptScn);
        ScnText::Status st;
        st.listening = true;
        emit scnStatus(st);
        return;
    }
    connect(m_server, &QTcpServer::newConnection, this, &LinkWorker::acceptTcp);
}

void LinkWorker::acceptScn()
{
    if (m_scn || !m_server || !m_server->hasPendingConnections())
        return;
    QTcpSocket *s = m_server->nextPendingConnection();
    // Một PC tại một thời điểm (anh Linh chốt): ngừng nhận để PC thứ hai nằm chờ
    // trong hàng đợi của hệ điều hành, giống SW1, cho tới khi PC đầu ngắt.
    m_server->pauseAccepting();
    m_scn = new ScnTextSession(s, this);
    connect(m_scn, &ScnTextSession::statusChanged, this, &LinkWorker::scnStatus);
    connect(m_scn, &ScnTextSession::lineReceived, this,
            [this](const QByteArray &line) { record(line.constData(), int(line.size())); });
    connect(m_scn, &ScnTextSession::unknownCommand, this, [this](const QString &line) {
        emit message(QStringLiteral("%1: PC gửi lệnh lạ, không trả lời: %2")
                         .arg(m_entry.category, line), false);
    });
    connect(m_scn, &ScnTextSession::finished, this, &LinkWorker::endScnSession);
    emit message(QStringLiteral("%1: PC %2 đã kết nối.")
                     .arg(m_entry.category, m_scn->status().peer), false);
    emit scnStatus(m_scn->status());
}

void LinkWorker::endScnSession()
{
    if (!m_scn)
        return;
    if (m_scn->timedOut()) {
        emit message(QStringLiteral("%1: PC %2 im lặng quá %3 giây (không có keepalive), bỏ phiên và chờ "
                                    "kết nối lại.")
                         .arg(m_entry.category, m_scn->status().peer)
                         .arg(ScnTextSession::kIdleTimeoutMs / 1000), true);
    } else {
        emit message(QStringLiteral("%1: PC %2 đã ngắt kết nối, chờ kết nối lại.")
                         .arg(m_entry.category, m_scn->status().peer), false);
    }
    m_scn->deleteLater();
    m_scn = nullptr;

    ScnText::Status st;
    st.listening = true;
    emit scnStatus(st);

    if (m_server) {
        m_server->resumeAccepting();
        acceptScn();
    }
}

void LinkWorker::openTcpClient()
{
    m_client = new QTcpSocket(this);
    m_tcpBuffers.insert(m_client, QByteArray());
    connect(m_client, &QTcpSocket::readyRead, this, &LinkWorker::readTcp);
    connect(m_client, &QTcpSocket::disconnected, this, &LinkWorker::reconnectTcp);

    m_retry = new QTimer(this);
    m_retry->setInterval(kRetryMs);
    connect(m_retry, &QTimer::timeout, this, &LinkWorker::reconnectTcp);
    m_retry->start();

    m_client->connectToHost(m_remoteAddr, m_entry.remotePort);
}

void LinkWorker::reconnectTcp()
{
    if (!m_client || m_client->state() != QAbstractSocket::UnconnectedState)
        return;
    m_tcpBuffers[m_client].clear();
    m_client->connectToHost(m_remoteAddr, m_entry.remotePort);
}

void LinkWorker::acceptTcp()
{
    while (m_server && m_server->hasPendingConnections()) {
        QTcpSocket *s = m_server->nextPendingConnection();
        m_tcpBuffers.insert(s, QByteArray());
        connect(s, &QTcpSocket::readyRead, this, &LinkWorker::readTcp);
        connect(s, &QTcpSocket::disconnected, this, [this, s] { dropTcpSocket(s); });
        emit message(QStringLiteral("%1: %2 đã kết nối.")
                         .arg(m_entry.category, s->peerAddress().toString()), false);
    }
}

void LinkWorker::dropTcpSocket(QTcpSocket *socket)
{
    if (!socket || socket == m_client)
        return;
    emit message(QStringLiteral("%1: %2 đã ngắt kết nối.")
                     .arg(m_entry.category, socket->peerAddress().toString()), false);
    m_tcpBuffers.remove(socket);
    socket->deleteLater();
}

bool LinkWorker::senderAllowed(const QHostAddress &addr, quint16 port) const
{
    // RemoteIP = 0.0.0.0 nhận từ mọi địa chỉ, RemotePort = 0 nhận từ mọi cổng.
    if (!m_remoteAddr.isNull() && m_remoteAddr != QHostAddress(QHostAddress::AnyIPv4)
        && addr != m_remoteAddr && !addr.isEqual(m_remoteAddr, QHostAddress::ConvertV4MappedToIPv4)) {
        return false;
    }
    return m_entry.remotePort == 0 || port == m_entry.remotePort;
}

void LinkWorker::readUdp()
{
    while (m_udp && m_udp->hasPendingDatagrams()) {
        if (m_rawSink) {
            if (!readRawDatagram())
                break;
            continue;
        }
        const QNetworkDatagram dg = m_udp->receiveDatagram();
        if (!senderAllowed(dg.senderAddress(), quint16(dg.senderPort())))
            continue;
        const QByteArray raw = dg.data();
        record(raw.constData(), int(raw.size()));
        handleDatagram(raw);
    }
}

void LinkWorker::handleDatagram(const QByteArray &raw)
{
    switch (m_format) {
    case LinkEntry::Dataframe:
        handleRaw(raw);
        return;
    case LinkEntry::ScnCf: {
        ScnCf::Message msg;
        QString error;
        if (ScnCf::decode(raw, &msg, &error))
            emit scnCfReceived(msg);
        else
            warnDecodeOnce(QStringLiteral("gói Cf hỏng (%1)").arg(error));
        return;
    }
    case LinkEntry::Asterix: {
        Asterix::Batch batch;
        QString error;
        if (!Asterix::decode(raw, &batch, &error))
            warnDecodeOnce(QStringLiteral("gói ASTERIX hỏng (%1)").arg(error));
        // Gói hỏng giữa chừng vẫn giữ các bản ghi đã đọc được trước chỗ hỏng.
        if (!batch.services.isEmpty() || !batch.reports.isEmpty())
            emit asterixReceived(batch);
        return;
    }
    default:
        // raw_iq thiếu nơi nhận (đã báo lúc mở cổng); scn_text không đi trên UDP.
        return;
    }
}

void LinkWorker::warnDecodeOnce(const QString &text)
{
    if (m_decodeWarned)
        return;
    m_decodeWarned = true;
    emit message(QStringLiteral("%1: %2. Các gói hỏng sau đó bị bỏ, không báo lại.")
                     .arg(m_entry.category, text), true);
}

bool LinkWorker::readRawDatagram()
{
    // Đọc thẳng vào bộ đệm dựng sẵn thay vì receiveDatagram(): 400 gói/giây
    // mà gói nào cũng cấp phát một QByteArray 19 KB thì chỉ tổ phân mảnh heap.
    QHostAddress addr;
    quint16 port = 0;
    const qint64 n = m_udp->readDatagram(m_rawBuffer.data(), m_rawBuffer.size(), &addr, &port);
    if (n < 0)
        return false;
    if (senderAllowed(addr, port)) {
        checkRawSize(n);
        record(m_rawBuffer.constData(), int(n));
        m_rawSink->feed(m_rawBuffer.constData(), int(n), m_bigEndian);
    }
    return true;
}

void LinkWorker::checkRawSize(qint64 size)
{
    if (m_rawSizeWarned || size == m_rawSink->frameBytes())
        return;
    // Báo một lần mỗi lần kết nối là đủ: gói lệch cỡ thường do cấu hình sai
    // phía hệ thống MH, lặp lại 400 lần/giây chỉ làm ngập bảng thông báo.
    m_rawSizeWarned = true;
    emit message(QStringLiteral("%1: nhận gói %2 byte, đặc tả là %3 byte.")
                     .arg(m_entry.category).arg(size).arg(m_rawSink->frameBytes()), true);
}

void LinkWorker::readTcp()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    QByteArray &buf = m_tcpBuffers[socket];
    buf.append(socket->readAll());

    if (m_format != LinkEntry::Dataframe && !m_rawSink) {
        // Định dạng không cắt được trên dòng TCP này: vứt để bộ đệm không phình.
        buf.clear();
        return;
    }
    if (m_rawSink) {
        // Gói thô không có header để dò đồng bộ, chỉ cắt đúng cỡ liên tiếp.
        const int n = m_rawSink->frameBytes();
        int offset = 0;
        while (buf.size() - offset >= n) {
            record(buf.constData() + offset, n);
            m_rawSink->feed(buf.constData() + offset, n, m_bigEndian);
            offset += n;
        }
        buf.remove(0, offset);
        return;
    }

    // TCP là dòng byte liên tục nên phải tự cắt gói theo trường length.
    while (buf.size() >= 12) {
        const int len = Proto::frameLength(buf, m_bigEndian);
        if (len < 0) {
            buf.remove(0, 1); // mất đồng bộ: trượt một byte đi tìm header
            continue;
        }
        if (len == 0 || buf.size() < len)
            break;
        record(buf.constData(), len);
        handleRaw(buf.left(len));
        buf.remove(0, len);
    }
}

void LinkWorker::handleRaw(const QByteArray &raw)
{
    // Header 2d2d2d2d đọc chiều nào cũng khớp, còn máy XL MH để length = 0 nên
    // gói ngược thứ tự byte vẫn mở được với category vô nghĩa rồi bị bỏ âm thầm
    // (lần thử thật 2026-10-08 mất cả buổi vì thế). Mọi category đều nhỏ hơn
    // 0x1000000: category đọc ra lớn mà đảo byte lại thì nhỏ là cấu hình ngược.
    if (raw.size() >= 8 && Proto::readU32(raw.constData(), m_bigEndian) == Proto::kHeader
        && Proto::readU32(raw.constData() + 4, m_bigEndian) > 0xffffffu
        && Proto::readU32(raw.constData() + 4, !m_bigEndian) <= 0xffffffu) {
        warnDecodeOnce(QStringLiteral("gói đến là %1 nhưng dòng đặt \"big_endian\": %2 — sửa khoá này trong "
                                      "settings/connect.json rồi chạy lại")
                           .arg(m_bigEndian ? QStringLiteral("little-endian") : QStringLiteral("big-endian"),
                                m_bigEndian ? QStringLiteral("true") : QStringLiteral("false")));
        return;
    }
    Proto::Frame frame;
    if (!Proto::parse(raw, &frame, m_bigEndian))
        return;
    emit frameReceived(frame.category, frame.serial, frame.data, m_bigEndian);
}

void LinkWorker::sendFrame(quint32 category, const QByteArray &data)
{
    Proto::Frame frame;
    frame.category = category;
    frame.serial = ++m_serial;
    frame.time = Proto::msOfDay();
    frame.data = data;
    // Chỉ báo "gửi thành công" khi byte thật sự ra khỏi socket: nhãn serial
    // trên cửa sổ kỹ sư dựa vào tín hiệu này.
    if (writeOut(Proto::build(frame, m_bigEndian)))
        emit frameSent(category, frame.serial);
}

void LinkWorker::sendRawBytes(const QByteArray &raw)
{
    writeOut(raw);
}

bool LinkWorker::writeOut(const QByteArray &raw)
{
    if (m_entry.protocol == LinkEntry::Udp) {
        if (!m_udp)
            return false;
        return m_udp->writeDatagram(raw, m_remoteAddr, m_entry.remotePort) == raw.size();
    }
    if (m_format == LinkEntry::ScnText)
        return m_scn && m_scn->write(raw);
    if (m_client && m_client->state() == QAbstractSocket::ConnectedState)
        return m_client->write(raw) == raw.size();

    bool any = false;
    for (auto it = m_tcpBuffers.constBegin(); it != m_tcpBuffers.constEnd(); ++it) {
        if (it.key()->state() == QAbstractSocket::ConnectedState)
            any = (it.key()->write(raw) == raw.size()) || any;
    }
    return any;
}

// ----------------------------------------------------------------- manager

LinkManager::LinkManager(QObject *parent)
    : QObject(parent)
{
    // Hai kiểu này đi qua hàng đợi tín hiệu từ luồng của cổng về luồng giao diện.
    qRegisterMetaType<ScnText::Status>();
    qRegisterMetaType<ScnCf::Message>();
    qRegisterMetaType<Asterix::Batch>();
}

LinkManager::~LinkManager()
{
    stop();
}

void LinkManager::start()
{
    if (isRunning())
        return;

    for (int i = 0; i < m_config.entries.size(); ++i) {
        const LinkEntry &entry = m_config.entries.at(i);
        auto *thread = new QThread(this);
        thread->setObjectName(QStringLiteral("link-%1").arg(entry.category));

        auto *worker = new LinkWorker(entry, m_rawSinks.value(entry.category));
        if (m_recordSink)
            worker->setRecordSink(m_recordSink, i);
        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &LinkWorker::begin);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &LinkWorker::frameReceived, this, &LinkManager::frameReceived);
        connect(worker, &LinkWorker::frameSent, this, &LinkManager::frameSent);
        connect(worker, &LinkWorker::message, this, &LinkManager::message);
        connect(worker, &LinkWorker::scnStatus, this, &LinkManager::scnStatus);
        connect(worker, &LinkWorker::scnCfReceived, this, &LinkManager::scnCfReceived);
        connect(worker, &LinkWorker::asterixReceived, this, &LinkManager::asterixReceived);

        m_threads.append(thread);
        m_workers.append(worker);
        m_byCategory.insert(entry.category, worker);

        thread->start();
    }
    emit message(QStringLiteral("Đã mở %1 cổng gửi/nhận dữ liệu.").arg(m_threads.size()), false);
}

void LinkManager::stop()
{
    if (!isRunning())
        return;

    for (LinkWorker *w : std::as_const(m_workers)) {
        // Đóng socket ngay trên luồng của nó rồi mới cho luồng thoát, nếu không
        // socket sẽ bị huỷ từ luồng khác và Qt cảnh báo.
        QMetaObject::invokeMethod(w, "finish", Qt::BlockingQueuedConnection);
    }
    for (QThread *t : std::as_const(m_threads)) {
        t->quit();
        t->wait(2000);
        delete t;
    }
    m_threads.clear();
    m_workers.clear();
    m_byCategory.clear();
}

bool LinkManager::send(const QString &category, quint32 packetCategory, const QByteArray &data)
{
    LinkWorker *worker = m_byCategory.value(category);
    if (!worker)
        return false;
    QMetaObject::invokeMethod(worker, "sendFrame", Qt::QueuedConnection,
                              Q_ARG(quint32, packetCategory), Q_ARG(QByteArray, data));
    return true;
}

void LinkManager::setRawSink(const QString &category, std::shared_ptr<RawSink> sink)
{
    m_rawSinks.insert(category, std::move(sink));
}

bool LinkManager::sendRaw(const QString &category, const QByteArray &raw)
{
    LinkWorker *worker = m_byCategory.value(category);
    if (!worker)
        return false;
    QMetaObject::invokeMethod(worker, "sendRawBytes", Qt::QueuedConnection,
                              Q_ARG(QByteArray, raw));
    return true;
}
