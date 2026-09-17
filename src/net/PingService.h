#pragma once

#include "core/Settings.h"

#include <QObject>
#include <QVector>

class QThread;

// Kiểm tra kết nối tới các nút mạng bằng lệnh ping của hệ điều hành.
//
// Qt không có API ICMP nên phải gọi tiến trình ping; mỗi lần gọi có thể treo
// tới vài giây, vì vậy các nút được chia thành nhóm nhỏ chạy trên luồng riêng để
// một nút chết không làm chậm việc kiểm tra các nút còn lại.
class PingWorker : public QObject
{
    Q_OBJECT
public:
    PingWorker(const QVector<int> &indices, const QVector<QString> &addresses, int periodMs);

public slots:
    void begin();
    void finish();

signals:
    void nodeState(int index, bool alive);

private slots:
    void tick();

private:
    QVector<int> m_indices;
    QVector<QString> m_addresses;
    int m_periodMs;
    class QTimer *m_timer = nullptr;
};

class PingService : public QObject
{
    Q_OBJECT
public:
    explicit PingService(QObject *parent = nullptr);
    ~PingService() override;

    // Mỗi luồng phụ trách tối đa 3 nút mạng.
    void start(const QVector<NetNode> &nodes);
    void stop();

    bool isAlive(int index) const;
    int overallLevel() const; // 0: tốt, 1: cảnh báo, 2: lỗi

signals:
    void nodeStateChanged(int index, bool alive);
    void overallLevelChanged(int level);

private:
    void onNodeState(int index, bool alive);

    QVector<NetNode> m_nodes;
    QVector<bool> m_alive;
    QVector<bool> m_known;
    QVector<QThread *> m_threads;
    int m_lastLevel = -1;
};
