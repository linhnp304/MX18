#pragma once

#include "proto/ScnText.h"

#include <QByteArray>
#include <QObject>

class QTcpSocket;
class QTimer;

// Một phiên làm việc với máy "PC" trên dòng X18-SCN (định dạng scn_text).
//
// Sống trên luồng của cổng và trả lời lệnh ngay tại đó, không vòng qua luồng
// giao diện: PC chờ câu trả lời, một lần vẽ lại bản đồ chậm không được làm trễ
// nó. Chỉ trạng thái mới gửi về giao diện.
//
// Vòng đời theo ý định của SW1 (code mẫu thiếu "break" nên chưa bao giờ chạy
// tới đây): PC kết nối → chờ 1 giây → gửi khối Start → trả lời từng dòng lệnh,
// và cứ 5 giây không gửi Start thì gửi lại. SW1 chỉ gửi lại khi có dòng mới
// đến; ở đây dùng bộ hẹn giờ thật để PC im lặng vẫn nhận được Start.
//
// Khác SW1: PC im lặng quá kIdleTimeoutMs thì bỏ phiên (anh Linh chốt
// 2026-10-08). PC thật gửi !keepalive mỗi ~1 giây; mất kết nối đột ngột (rút
// cáp, mất điện) không có FIN nên TCP chỉ bỏ cuộc sau nhiều phút, mà trong lúc
// đó MX18 chỉ phục vụ một PC — PC nối lại sẽ nằm chờ không ai trả lời.
class ScnTextSession : public QObject
{
    Q_OBJECT
public:
    // Nhận quyền sở hữu socket đã kết nối.
    explicit ScnTextSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~ScnTextSession() override;

    static constexpr int kIdleTimeoutMs = 10000;

    QTcpSocket *socket() const { return m_socket; }
    const ScnText::Status &status() const { return m_status; }
    // Phiên kết thúc vì PC im lặng quá lâu chứ không phải PC tự ngắt.
    bool timedOut() const { return m_timedOut; }

    bool write(const QByteArray &text);

signals:
    void statusChanged(const ScnText::Status &status);
    // Mỗi dòng lệnh PC gửi đến (đã cắt khoảng trắng), để ghi lưu.
    void lineReceived(const QByteArray &line);
    // Lệnh không có trong bảng của SW1 (không trả lời) — báo để anh Linh ghi lại
    // khi thử với hệ thống thật.
    void unknownCommand(const QString &line);
    // PC đã ngắt; lớp gọi huỷ phiên rồi chờ kết nối mới.
    void finished();

private:
    void readLines();
    void handleLine(const QByteArray &line);
    void sendStart();
    void finish();

    QTcpSocket *m_socket = nullptr;
    QTimer *m_startTimer = nullptr;
    QTimer *m_idleTimer = nullptr;
    QByteArray m_buffer;
    ScnText::State m_state;
    ScnText::Status m_status;
    bool m_finished = false;
    bool m_timedOut = false;
};
