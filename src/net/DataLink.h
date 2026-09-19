#pragma once

#include "net/LinkConfig.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QVector>

class QTcpServer;
class QTcpSocket;
class QThread;
class QTimer;
class QUdpSocket;

// Một cổng gửi/nhận, chạy trên luồng riêng.
//
// Đường quét RD và MH mỗi loại khoảng 400 gói tin/giây; nếu gộp chung vào luồng
// giao diện thì một lần vẽ lại bản đồ cũng đủ làm rơi gói. Vì vậy mỗi dòng trong
// connect.json có một luồng riêng, chỉ kết quả đã mở gói mới đi qua hàng đợi tín
// hiệu về luồng giao diện.
class LinkWorker : public QObject
{
    Q_OBJECT
public:
    LinkWorker(const LinkEntry &entry, bool bigEndian);
    ~LinkWorker() override;

public slots:
    void begin();
    void finish();
    void sendFrame(quint32 category, const QByteArray &data);

signals:
    // serial là trường của khung gói tin, cửa sổ "Trạng thái MH" hiển thị nó.
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data);
    void message(const QString &text, bool isError);

private slots:
    void readUdp();
    void acceptTcp();
    void readTcp();
    void reconnectTcp();

private:
    void openUdp();
    void openTcpServer();
    void openTcpClient();
    void handleRaw(const QByteArray &raw);
    bool senderAllowed(const QHostAddress &addr, quint16 port) const;
    void dropTcpSocket(QTcpSocket *socket);

    LinkEntry m_entry;
    bool m_bigEndian;
    quint32 m_serial = 0;

    QUdpSocket *m_udp = nullptr;
    QTcpServer *m_server = nullptr;
    QTcpSocket *m_client = nullptr;          // chỉ dùng khi TCP Client
    QTimer *m_retry = nullptr;
    QHash<QTcpSocket *, QByteArray> m_tcpBuffers;

    QHostAddress m_remoteAddr;
    QHostAddress m_localAddr;
};

// Quản lý toàn bộ các cổng theo connect.json, sống trên luồng giao diện.
class LinkManager : public QObject
{
    Q_OBJECT
public:
    explicit LinkManager(QObject *parent = nullptr);
    ~LinkManager() override;

    void setConfig(const LinkConfig &config) { m_config = config; }
    const LinkConfig &config() const { return m_config; }

    bool isRunning() const { return !m_threads.isEmpty(); }

    void start();
    void stop();

    // Gửi một gói tin qua dòng cấu hình mang tên phân loại này.
    void send(const QString &category, quint32 packetCategory, const QByteArray &data);

signals:
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data);
    void message(const QString &text, bool isError);

private:
    LinkConfig m_config;
    QVector<QThread *> m_threads;
    QVector<LinkWorker *> m_workers;
    QHash<QString, LinkWorker *> m_byCategory;
};
