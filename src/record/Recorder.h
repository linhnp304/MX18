#pragma once

#include "core/Settings.h"
#include "record/RecordFormat.h"

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

#include <array>
#include <atomic>
#include <memory>

class QFile;
class QThread;
class QTimer;
struct LinkConfig;

// Số liệu file đang ghi cho tab "Ghi lưu" (cập nhật 200 ms).
struct RecordStats {
    bool active = false;
    QString fileName;          // đường dẫn tương đối trong ./records
    qint64 elapsedMs = 0;      // từ lúc mở file này
    quint64 packets = 0;
    quint64 bytes = 0;         // kể cả phần còn trong bộ đệm chờ ghi
};

// Chỗ các luồng nhận đẩy gói thô vào, dùng chung giữa mọi LinkWorker và luồng
// ghi (shared_ptr như RawSink để không ai giữ con trỏ treo).
//
// push() chạy ngay trên luồng của cổng nhận nên chỉ khoá mutex, nối vào bộ đệm
// rồi nhả; ghi đĩa để luồng ghi làm theo nhịp. Thời điểm gói lấy dưới cùng một
// mutex nên thứ tự trong file luôn tăng dần dù gói đến từ nhiều luồng — phát lại
// và tua dựa vào điều đó.
class RecordSink
{
public:
    RecordSink();

    // Đọc không khoá: lúc không ghi lưu mỗi gói chỉ tốn một lần đọc nguyên tử.
    bool isActive() const { return m_active.load(std::memory_order_relaxed); }
    void push(int stream, const char *data, int size);

    RecordStats stats() const;

private:
    friend class RecordWriter;
    friend class Recorder;

    enum Policy { Full, Short, Skip };
    struct Stream {
        int kind = RecordFormat::KindOther;
        bool bigEndian = false;
        Policy policy = Skip;  // dòng chỉ gửi không bao giờ có gói đẩy vào
        bool recv = false;
    };

    // Phần luồng ghi lấy ra mỗi nhịp.
    struct Take {
        QByteArray packets;
        quint32 count = 0;
        quint32 lastMs = 0;
        std::array<quint32, RecordFormat::KindCount> kinds{};
        bool overflow = false;
        // Khi ngắt file: mốc của file mới, lấy cùng lúc đổi bộ đệm.
        qint64 newStartMs = 0;
        QDateTime newStartWall;
    };

    void setPolicies(const RecordSetup &setup);
    // Mở nhận cho file mới bắt đầu ở mốc này.
    void open(qint64 startMs, const QString &fileName, quint64 fileBytes);
    void setFileName(const QString &fileName, quint64 fileBytes);
    // Đổi bộ đệm; rotate thì gói đến sau thuộc file mới bắt đầu từ bây giờ.
    Take take(bool rotate);
    // Ngừng nhận và lấy nốt phần còn lại (dừng ghi lưu).
    Take close();
    void addBytes(quint64 n);
    qint64 nowMs() const { return m_clock.elapsed(); }

    QElapsedTimer m_clock;
    std::atomic<bool> m_active{false};

    mutable QMutex m_mutex;
    QVector<Stream> m_streams;
    bool m_accepting = false;
    qint64 m_fileStartMs = 0;
    QByteArray m_pending;
    Take m_counts;             // chỉ dùng count / lastMs / kinds / overflow
    QString m_fileName;
    quint64 m_filePackets = 0;
    quint64 m_fileBytes = 0;
};

// Sống trên luồng ghi riêng: nhịp kTickMs đổi bộ đệm rồi ghi một khối, ngắt
// file theo dung lượng / thời gian, định kỳ xoá file cũ. Mọi lỗi thành tín hiệu
// failed(), không throw, không abort — ghi lưu hỏng không được kéo phần mềm theo.
class RecordWriter : public QObject
{
    Q_OBJECT
public:
    RecordWriter(std::shared_ptr<RecordSink> sink);
    ~RecordWriter() override;

    // Gọi trên luồng ghi. Trả về mô tả lỗi, rỗng khi đã mở file.
    QString beginSession(const RecordSetup &setup, const QJsonArray &streams);
    QString endSession();

signals:
    void message(const QString &text, bool isError);
    void failed(const QString &text);
    // File đầu tiên của lần ghi đã mở; ngắt sang file mới báo bằng message().
    void fileOpened(const QString &name);

private:
    void tick();
    bool openFile(qint64 startMs, const QDateTime &wall, QString *error);
    bool writeTake(const RecordSink::Take &take, QString *error);
    bool writeHeader(bool closing, QString *error);
    void closeFile(bool closing);
    void fail(const QString &text);
    void cleanup();
    QString writeError(const QString &what) const;

    std::shared_ptr<RecordSink> m_sink;
    QTimer *m_timer = nullptr;
    QFile *m_file = nullptr;
    QString m_relName;
    RecordFormat::Header m_header;
    RecordSetup m_setup;
    QJsonArray m_streams;
    qint64 m_fileStartMs = 0;
    quint64 m_written = 0;
    quint64 m_maxBytes = 0;
    qint64 m_maxMs = 0;
    int m_ticks = 0;
    bool m_capWarned = false;
};

// Mặt tiền trên luồng giao diện: điều khiển luồng ghi và chuyển tín hiệu về.
class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    // Bảng dòng theo connect.json của lần chạy này; chỉ số dòng trong file ghi
    // lưu là chỉ số trong config.entries (LinkManager đưa đúng số đó cho worker).
    void setStreams(const LinkConfig &config);
    std::shared_ptr<RecordSink> sink() const { return m_sink; }

    bool isRecording() const { return m_recording; }
    // Chặn tới khi luồng ghi mở xong file đầu tiên.
    bool start(const RecordSetup &setup, QString *error);
    // Ghi nốt bộ đệm, đóng file. Trả về mô tả lỗi nếu ghi nốt hỏng.
    QString stop();
    RecordStats stats() const { return m_sink->stats(); }

signals:
    void message(const QString &text, bool isError);
    // Ghi lưu tự dừng vì lỗi.
    void failed(const QString &text);
    void fileOpened(const QString &name);

private:
    std::shared_ptr<RecordSink> m_sink;
    QThread *m_thread = nullptr;
    RecordWriter *m_writer = nullptr;
    QJsonArray m_streams;
    bool m_recording = false;
};
