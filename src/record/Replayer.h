#pragma once

#include "proto/Asterix.h"
#include "proto/ScnCf.h"
#include "record/RecordFormat.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>

class LinkWorker;
class QFile;
class QThread;
class QTimer;
class RawSink;

// Số liệu cho nhãn của nhóm "Phát lại" (200 ms).
struct ReplayStats {
    qint64 posMs = 0;          // vị trí đang phát, ms từ đầu file
    qint64 durationMs = 0;     // ms từ đầu file tới gói cuối
    qint64 wallMs = 0;         // giờ lúc ghi của vị trí đang phát (ms từ epoch)
    // Đếm từ lúc bấm "Phát lại", tua không đặt lại.
    quint64 total = 0;
    quint64 videoR = 0;
    quint64 videoI = 0;
    quint64 plot = 0;
    quint64 track = 0;         // datagram X18-VQ + gói TRACK trên Data-Status
    bool paused = false;
    bool atEnd = false;
};

// Phần dùng chung giữa luồng phát lại và luồng giao diện.
struct ReplayShared {
    mutable QMutex mutex;
    ReplayStats stats;
    // Số nhịp luồng giao diện đã xử lý xong; luồng phát lại thấy giao diện tụt
    // lại quá xa thì đứng chờ thay vì dồn gói vào hàng đợi tín hiệu.
    std::atomic<quint64> acked{0};
};

// Sống trên luồng phát lại: đọc file theo khối, đẩy gói qua đúng LinkWorker
// của dòng đã ghi (bảng dòng lấy từ khối mô tả, không theo connect.json hiện
// tại) theo nhịp thời gian của file nhân tốc độ.
class ReplayEngine : public QObject
{
    Q_OBJECT
public:
    ReplayEngine(std::shared_ptr<ReplayShared> shared, std::shared_ptr<RawSink> rawSink);
    ~ReplayEngine() override;

    // Gọi trên luồng phát lại. Trả về mô tả lỗi, rỗng khi đã mở và bắt đầu phát.
    QString open(const QString &path);
    void close();
    void setPaused(bool paused);
    void seek(qint64 ms);
    void setRate(double rate);

signals:
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data, bool bigEndian);
    void scnCfReceived(const ScnCf::Message &message);
    void asterixReceived(const Asterix::Batch &batch);
    void message(const QString &text, bool isError);
    // Mỗi nhịp phát (kể cả không có gói), đi sau các gói của nhịp đó.
    void tick(quint64 seq, qint64 fileMs, qint64 wallMs);
    // Vừa tua: giao diện xoá quỹ đạo / điểm dấu / video trước khi gói mới tới.
    void seeked();
    void reachedEnd();

private:
    void step();
    void markEnd();
    bool peek();
    bool loadBlock(int index);
    void deliver(const RecordFormat::Packet &pk);
    void publish();

    std::shared_ptr<ReplayShared> m_shared;
    std::shared_ptr<RawSink> m_rawSink;
    QTimer *m_timer = nullptr;
    QFile *m_file = nullptr;

    QVector<RecordFormat::BlockRef> m_blocks;
    int m_blockIdx = -1;
    QByteArray m_block;        // thân khối đang đọc (bỏ 16 byte đầu khối)
    int m_offset = 0;
    RecordFormat::Packet m_peek;
    bool m_havePeek = false;

    QHash<int, LinkWorker *> m_workers;
    QVector<int> m_streamKind;
    QVector<bool> m_streamBe;

    qint64 m_startWallMs = 0;
    qint64 m_durationMs = 0;
    double m_posMs = 0.0;
    double m_rate = 1.0;
    bool m_paused = false;
    bool m_atEnd = false;
    QElapsedTimer m_real;
    quint64 m_seq = 0;
    ReplayStats m_counts;
};

// Mặt tiền trên luồng giao diện: dựng / dừng luồng phát lại, chuyển tín hiệu
// của nó về, giữ DataClock bám theo thời điểm của gói vừa phát.
class Replayer : public QObject
{
    Q_OBJECT
public:
    // rawSink nhận gói của dòng raw_iq (Data-RAW) như lúc chạy thật.
    explicit Replayer(std::shared_ptr<RawSink> rawSink, QObject *parent = nullptr);
    ~Replayer() override;

    bool isActive() const { return m_thread != nullptr; }
    // Chặn tới khi mở xong file (dựng bảng khối).
    bool start(const QString &path, QString *error);
    void stop();

    void setPaused(bool paused);
    void seek(qint64 ms);
    void setRate(double rate);
    ReplayStats stats() const;

signals:
    void frameReceived(quint32 category, quint32 serial, const QByteArray &data, bool bigEndian);
    void scnCfReceived(const ScnCf::Message &message);
    void asterixReceived(const Asterix::Batch &batch);
    void message(const QString &text, bool isError);
    // Đồng hồ dữ liệu đã nhảy (DataClock::jump), lớp gọi xoá phần đang vẽ.
    void seeked();
    void reachedEnd();

private:
    std::shared_ptr<RawSink> m_rawSink;
    std::shared_ptr<ReplayShared> m_shared;
    QThread *m_thread = nullptr;
    ReplayEngine *m_engine = nullptr;
    // Chọn trước khi bấm "Phát lại" thì áp dụng ngay từ gói đầu tiên.
    double m_rate = 1.0;
    // Tăng mỗi lần dừng: tín hiệu của lần phát trước còn nằm trong hàng đợi thì
    // bị bỏ, không vẽ đè lên giao diện vừa trả lại.
    quint64 m_generation = 0;
};
