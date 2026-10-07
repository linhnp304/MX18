#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

// Giao thức văn bản của dòng X18-SCN (TCP 10555): MX18 đóng vai thiết bị SCN,
// máy "PC" 192.168.232.1 kết nối vào và gửi lệnh "!… &", SCN trả lời "#… &".
//
// Mọi câu trả lời chép nguyên văn từ SW1 (docs/sample_code/SW1/SCN/SCN_TCP.cs,
// phân tích ở analysis-results/01-x18-scn.md): các con số như "#scnrpmode 11"
// cho lệnh "!scnrpmode 3" nhiều khả năng lấy từ nhật ký thiết bị SCN thật, nên
// sai một ký tự là PC có thể không nhận.
namespace ScnText {

// Biến trạng thái PC đặt qua lệnh. Giá trị mặc định theo SW1.
struct State {
    QByteArray scnrpmode = "3";
    QByteArray scnrpband = "1";
    QByteArray scnselrprz = "0";
    QByteArray prPower = "1";      // 27V
};

struct Reply {
    QByteArray text;               // các dòng trả lời, dòng nào cũng đã có \r\n
    bool sendStart = false;        // gửi thêm khối Start ngay sau câu trả lời
    bool known = false;            // lệnh có trong bảng của SW1
    QByteArray keepalive;          // số N của dòng "!keepalive", rỗng nếu không phải
};

// Khối "Start" gửi 1 giây sau khi PC kết nối và lặp lại mỗi 5 giây. Dòng
// "#scnrpcorr" cuối khối thiếu giá trị và dấu "&" — anh Linh chốt giữ nguyên
// như SW1, khi thử với hệ thống thật sẽ để ý dòng này.
QByteArray startBlock();

// Trả lời một dòng PC gửi (đã cắt ký tự xuống dòng). Lệnh không có trong bảng
// thì text rỗng và known = false — SW1 không trả lời các lệnh đó.
Reply answer(const QByteArray &line, State *state);

// Các dòng tự báo "*…" của SW1. Viết sẵn theo yêu cầu của anh Linh nhưng chưa
// gọi ở đâu, giống SW1.
QByteArray infBlock(const State &state);
QByteArray azimuthLine(const QByteArray &azimuth);

// Trạng thái của phiên làm việc, gửi về luồng giao diện để hiện lên panel 3.
struct Status {
    bool listening = false;        // cổng 10555 đã mở
    bool connected = false;
    QString peer;                  // địa chỉ của PC đang kết nối
    QString keepalive;             // N của dòng keepalive gần nhất
    QString scnrpmode, scnrpband, scnselrprz;
    quint32 linesIn = 0;           // số dòng PC gửi trong phiên này
    QString unknownLine;           // lệnh lạ mới nhất (không trả lời)
};

} // namespace ScnText

Q_DECLARE_METATYPE(ScnText::Status)
