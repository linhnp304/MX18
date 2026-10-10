#pragma once

#include <QDateTime>
#include <QtGlobal>

// Đồng hồ của dữ liệu, chỉ dùng trên luồng giao diện.
//
// Mọi phép tính theo thời gian của dữ liệu (xoá quỹ đạo không cập nhật, bộ bám
// MH đo chu kỳ quét và vận tốc, điểm dấu hết hạn, video mờ dần, chu kỳ quay
// trong North marker) đọc đồng hồ này thay vì QElapsedTimer riêng. Lúc chạy thật
// nó trôi đúng như đồng hồ máy; lúc phát lại nó bám thời điểm của gói vừa phát,
// nên chạy nhanh 8x thì nhanh theo, tạm dừng thì đứng — nếu không, vận tốc bộ bám
// MH đo được ở 8x sẽ gấp 8 lần thật và quỹ đạo bị xoá giữa lúc tạm dừng.
//
// nowMs() không bao giờ lùi, kể cả khi tua về trước: tua thì lớp gọi xoá quỹ
// đạo / điểm dấu / video rồi chạy tiếp từ đúng mốc đang có.
namespace DataClock {

// ms đơn điệu, gốc tuỳ ý (chỉ dùng để lấy hiệu).
qint64 nowMs();
// Giờ của dữ liệu: giờ máy lúc chạy thật, giờ lúc ghi khi phát lại.
QDateTime wallNow();

bool replaying();
void beginReplay();
// Thời điểm (ms từ đầu file) và giờ ghi của dữ liệu vừa phát, gọi theo đúng thứ
// tự gói đến luồng giao diện.
void setReplayTime(qint64 fileMs, qint64 wallMsSinceEpoch);
// Sau khi tua: lần setReplayTime() kế tiếp nối tiếp từ mốc hiện tại.
void jump();
// Về chạy thật, nối tiếp từ mốc cuối của lúc phát lại.
void endReplay();

} // namespace DataClock
