#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Cấu hình các cổng gửi/nhận trong ./settings/connect.json.
//
// Kỹ sư sửa bảng này trong cửa sổ "Điều khiển và thiết lập mức kỹ sư" (tab
// "Connect"); phần mềm chỉ đọc lúc khởi động nên đổi xong phải chạy lại. Riêng
// hai khoá "format" và "big_endian" của từng dòng chỉ sửa được trong file (anh
// Linh chốt): đổi nhầm là cả luồng mất dữ liệu nên không đưa lên giao diện.

struct LinkEntry {
    enum Direction { Recv = 0, Send, SendRecv };
    // Định dạng gói quyết định cách cắt gói và bộ giải mã của dòng. Các luồng
    // X18-* theo chuẩn riêng của hệ thống bên kia (ASTERIX, giao thức SCN) chứ
    // không theo Dataframe quy ước, nên mỗi dòng mang định dạng của nó.
    enum Format { Dataframe = 0, RawIq, ScnText, ScnCf, Asterix, FormatCount };
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
    int format = Dataframe;
    // ASTERIX luôn big-endian, "Cf" của SCN luôn little-endian; dòng của hệ
    // thống MH mặc định big-endian (anh Linh chốt ở giai đoạn 2). Với scn_text
    // khoá này không có tác dụng.
    bool bigEndian = true;

    static QString formatName(int format);
    // -1 khi tên lạ.
    static int formatFromName(const QString &name);
    // Định dạng mặc định của một phân loại; phân loại lạ thì là Dataframe.
    static int defaultFormat(const QString &category);
    static bool defaultBigEndian(int format) { return format != ScnCf; }
};

struct LinkConfig {
    QVector<LinkEntry> entries;

    // Đọc ./settings/connect.json. Thiếu file thì dựng các dòng mặc định và ghi
    // ra; có file mà lỗi thì trả cấu hình mặc định kèm mô tả lỗi trong *error.
    // File của bản cũ (khoá big_endian chung, thiếu dòng) được chuyển sang dạng
    // mới rồi ghi lại, *note nhận câu báo cho trắc thủ.
    static LinkConfig load(QString *error = nullptr, QString *note = nullptr);
    static LinkConfig defaults();

    bool save() const;

    const LinkEntry *find(const QString &category) const;
    // Thứ tự byte của dòng mang tên phân loại này; không có dòng thì big-endian.
    bool bigEndianFor(const QString &category) const;

    // Danh sách tên phân loại để đổ vào ComboBox cột "Phân loại".
    static QStringList categoryNames();
};
