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
class ScnTextSession : public QObject
{
    Q_OBJECT
public:
    // Nhận quyền sở hữu socket đã kết nối.
    explicit ScnTextSession(QTcpSocket *socket, QObject *parent = nullptr);
    ~ScnTextSession() override;

    QTcpSocket *socket() const { return m_socket; }
    const ScnText::Status &status() const { return m_status; }

    bool write(const QByteArray &text);

signals:
    void statusChanged(const ScnText::Status &status);
    // Lệnh không có trong bảng của SW1 (không trả lời) — báo để anh Linh ghi lại
    // khi thử với hệ thống thật.
    void unknownCommand(const QString &line);
    // PC đã ngắt; lớp gọi huỷ phiên rồi chờ kết nối mới.
    void finished();

private:
    void readLines();
    void handleLine(const QByteArray &line);
    void sendStart();

    QTcpSocket *m_socket = nullptr;
    QTimer *m_startTimer = nullptr;
    QByteArray m_buffer;
    ScnText::State m_state;
    ScnText::Status m_status;
    bool m_finished = false;
};
