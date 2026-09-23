#pragma once

#include "net/DataLink.h"
#include "proto/RawIq.h"

#include <QMutex>

#include <atomic>
#include <vector>

// Chỗ hẹn giữa luồng nhận "Data-RAW" và cửa sổ "ViewIQ - Vẽ cánh sóng".
//
// Gói được tính ngay trên luồng nhận rồi chỉ giữ lại đúng phần cần vẽ: ViewIQ
// giữ gói mới nhất, Vẽ CS giữ một cặp Sum/Sub cho mỗi phương vị encoder. Cửa
// sổ chép bản chụp theo nhịp vẽ của nó; gói nào về giữa hai nhịp vẽ mà bị gói
// sau đè lên thì coi như bỏ qua. Nhờ vậy bộ nhớ không bao giờ tăng theo lưu
// lượng, và giao diện vẽ chậm chỉ làm thưa hình chứ không làm treo.
class RawIqStore : public RawSink
{
public:
    RawIqStore();

    int frameBytes() const override { return RawIq::kBytes; }
    void feed(const char *raw, int size, bool bigEndian) override;

    // --- gọi từ luồng giao diện

    // Start/Stop của cửa sổ. Lúc dừng, luồng nhận vứt gói ngay sau khi đọc,
    // không tích gì vào bộ nhớ. Chạy lại thì xoá đồ thị cũ: mỗi lần Start là
    // một lần đo cánh sóng mới.
    void setRunning(bool running);
    // StartWord / MeanWords; luồng nhận tự kẹp lại theo loại của từng gói vì
    // DataType trên giao diện bám theo IQType chậm mất một nhịp vẽ.
    void setWindow(int startWord, int meanWords);

    struct Snapshot {
        quint64 seq = 0;                // tăng mỗi gói hợp lệ
        int type = 0;                   // IQType của gói mới nhất, 0 = chưa có
        int azm4096 = 0;

        quint64 viewSeq = 0;
        int viewType = 0;
        int viewStart = 0;              // StartWord đã kẹp, đúng như lúc tách
        std::vector<qint16> iq1, iq2;   // kViewPoints điểm, rỗng = chưa có

        quint64 beamSeq = 0;
        int beamType = 0;
        int beamLast = -1;              // ô phương vị vừa ghi = vị trí đường quét
        std::vector<double> sum, sub;   // kAzimuthSteps ô, NaN = chưa có
    };

    // Chép phần mới hơn *out vào *out (mảng nào không đổi thì không chép).
    // false nếu từ lần trước đến giờ không có gì mới.
    bool update(Snapshot *out) const;

private:
    void clearLocked();

    std::atomic<bool> m_running{false};
    std::atomic<int> m_start{RawIq::kStartDefault};
    std::atomic<int> m_mean{RawIq::kMeanDefault};

    mutable QMutex m_mutex;
    Snapshot m_data;                    // chỉ đụng tới khi giữ m_mutex
};
