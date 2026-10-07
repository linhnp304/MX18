#pragma once

#include "net/LinkConfig.h"
#include "proto/ScnCf.h"
#include "proto/ScnText.h"

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QVector>

#include <memory>

class QTcpServer;
class QTcpSocket;
class QThread;
class QTimer;
class QUdpSocket;
class ScnTextSession;

// Nơi nhận gói thô không theo khung Dataframe (RAW_IQ của dòng "Data-RAW").
//
// feed() chạy ngay trên luồng của cổng nhận chứ không qua hàng đợi tín hiệu:
// RAW_IQ về đến 400 gói/giây, mỗi gói 19 KB, chỉ một lần vẽ lại chậm là hàng
// đợi về luồng giao diện phình ra không giới hạn. Vì vậy lớp cài đặt phải an
// toàn đa luồng và không được chặn lâu.
class RawSink
{
public:
    virtual ~RawSink() = default;
    // Cỡ một gói theo đặc tả: dòng TCP được cắt theo cỡ này, còn datagram UDP
    // lệch cỡ thì báo một lần để kỹ sư biết vì sao không thấy dữ liệu.
    virtual int frameBytes() const = 0;
    virtual void feed(const char *raw, int size, bool bigEndian) = 0;
};

// Một cổng gửi/nhận, chạy trên luồng riêng.
//
// Đường quét RD và MH mỗi loại khoảng 400 gói tin/giây; nếu gộp chung vào luồng
// giao diện thì một lần vẽ lại bản đồ cũng đủ làm rơi gói. Vì vậy mỗi dòng trong
// connect.json có một luồng riêng, chỉ kết quả đã mở gói mới đi qua hàng đợi tín
// hiệu về luồng giao diện.
//
// Cách cắt gói và giải mã theo khoá "format" của dòng:
//   dataframe — khung Dataframe quy ước (TCP cắt theo trường length)
//   raw_iq    — gói cỡ cố định, đưa thẳng vào RawSink trên luồng này
//   scn_text  — TCP Server một PC, mỗi dòng một lệnh (ScnTextSession)
//   scn_cf    — mỗi datagram một gói "Cf"; dòng gửi phát byte "0" lúc mở
//   asterix   — mỗi datagram một hay nhiều khối ASTERIX
class LinkWorker : public QObject
{
    Q_OBJECT
public:
    // rawSink chỉ dùng khi định dạng của dòng là raw_iq.
    explicit LinkWorker(const LinkEntry &entry, std::shared_ptr<RawSink> rawSink = {});
    ~LinkWorker() override;

public slots:
    void begin();
    void finish();
    void sendFrame(quint32 category, const QByteArray &data);
    // Gửi nguyên xi một chuỗi byte, không đóng khung gói tin (lệnh khởi động
    // lại hệ thống MH chỉ có 4 byte và không theo Dataframe; gói ASTERIX, "Cf").
    void sendRawBytes(const QByteArray &raw);

signals:
    // serial là trường của khung gói tin, cửa sổ "Trạng thái MH" hiển thị nó.
    // bigEndian là thứ tự byte của dòng nhận, lớp gọi mở các trường theo nó.
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data, bool bigEndian);
    void scnStatus(const ScnText::Status &status);
    void scnCfReceived(const ScnCf::Message &message);
    // Gửi xong một gói lệnh: các tab mức kỹ sư hiện serial vừa gửi.
    void frameSent(quint32 category, quint32 serial);
    void message(const QString &text, bool isError);

private slots:
    void readUdp();
    void acceptTcp();
    void readTcp();
    void reconnectTcp();
    void acceptScn();
    void endScnSession();

private:
    void openUdp();
    void openTcpServer();
    void openTcpClient();
    void handleRaw(const QByteArray &raw);
    void handleDatagram(const QByteArray &raw);
    // Báo lỗi giải mã một lần mỗi lần mở cổng: gói hỏng thường lặp lại đều đặn,
    // báo từng gói chỉ làm ngập bảng thông báo.
    void warnDecodeOnce(const QString &text);
    // false khi không đọc được datagram nào nữa (vòng đọc phải dừng).
    bool readRawDatagram();
    void checkRawSize(qint64 size);
    // false khi socket chưa mở hoặc chưa có đầu bên kia: lệnh không đi được.
    bool writeOut(const QByteArray &raw);
    bool senderAllowed(const QHostAddress &addr, quint16 port) const;
    void dropTcpSocket(QTcpSocket *socket);

    LinkEntry m_entry;
    bool m_bigEndian;
    int m_format;
    quint32 m_serial = 0;
    bool m_decodeWarned = false;

    QUdpSocket *m_udp = nullptr;
    QTcpServer *m_server = nullptr;
    QTcpSocket *m_client = nullptr;          // chỉ dùng khi TCP Client
    QTimer *m_retry = nullptr;
    QHash<QTcpSocket *, QByteArray> m_tcpBuffers;
    ScnTextSession *m_scn = nullptr;         // scn_text: PC đang kết nối

    QHostAddress m_remoteAddr;
    QHostAddress m_localAddr;

    std::shared_ptr<RawSink> m_rawSink;
    QByteArray m_rawBuffer;
    bool m_rawSizeWarned = false;
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

    // Gửi một gói tin qua dòng cấu hình mang tên phân loại này. Trả về false
    // khi chưa kết nối hoặc connect.json không có dòng nào mang tên đó — lớp
    // gọi báo lại cho người dùng thay vì im lặng nuốt lệnh.
    bool send(const QString &category, quint32 packetCategory, const QByteArray &data);
    bool sendRaw(const QString &category, const QByteArray &raw);

    // Gói nhận trên dòng mang tên phân loại này (định dạng raw_iq) đi thẳng vào
    // sink, không mở khung Dataframe. Có tác dụng từ lần start() kế tiếp.
    void setRawSink(const QString &category, std::shared_ptr<RawSink> sink);

signals:
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data, bool bigEndian);
    void frameSent(quint32 category, quint32 serial);
    void message(const QString &text, bool isError);
    void scnStatus(const ScnText::Status &status);
    void scnCfReceived(const ScnCf::Message &message);

private:
    LinkConfig m_config;
    QVector<QThread *> m_threads;
    QVector<LinkWorker *> m_workers;
    QHash<QString, LinkWorker *> m_byCategory;
    QHash<QString, std::shared_ptr<RawSink>> m_rawSinks;
};
