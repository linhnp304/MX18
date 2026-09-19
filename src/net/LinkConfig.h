#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Cấu hình các cổng gửi/nhận trong ./settings/connect.json.
//
// Kỹ sư sửa bảng này trong cửa sổ "Điều khiển và thiết lập mức kỹ sư" (tab
// "Connect"); phần mềm chỉ đọc lúc khởi động nên đổi xong phải chạy lại.

struct LinkEntry {
    enum Direction { Recv = 0, Send, SendRecv };
    enum Protocol { Udp = 0, Tcp };
    // Ý nghĩa của Type phụ thuộc Protocol: TCP là Server/Client, UDP là
    // Unicast/Broadcast. Dùng chung một số để bảng giao diện chỉ phải đổi danh
    // sách chữ trong ComboBox chứ không đổi kiểu dữ liệu.
    enum Type { ServerOrUnicast = 0, ClientOrBroadcast };

    QString category;
    int direction = Recv;
    int protocol = Udp;
    int type = ServerOrUnicast;
    QString localIp = QStringLiteral("0.0.0.0");
    quint16 localPort = 0;
    QString remoteIp = QStringLiteral("0.0.0.0");
    quint16 remotePort = 0;
};

struct LinkConfig {
    QVector<LinkEntry> entries;
    // Đặc tả không nói thứ tự byte của gói tin; anh Linh chốt mặc định
    // big-endian và cho đổi tại đây nếu hệ thống MH dùng little-endian.
    bool bigEndian = true;

    // Đọc ./settings/connect.json. Thiếu file thì dựng 9 dòng mặc định và ghi
    // ra; có file mà lỗi thì trả cấu hình mặc định kèm mô tả lỗi trong *error.
    static LinkConfig load(QString *error = nullptr);
    static LinkConfig defaults();

    bool save() const;

    const LinkEntry *find(const QString &category) const;

    // Danh sách tên phân loại để đổ vào ComboBox cột "Phân loại".
    static QStringList categoryNames();
};
