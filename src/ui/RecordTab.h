#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class Recorder;

// Tab "Ghi lưu" của panel 2 (step-07): nhóm "Ghi lưu" và nhóm "Phát lại".
// Bắt đầu / dừng do MainWindow quyết (đọc records.json, báo thông báo hệ thống);
// tab chỉ phát yêu cầu và tự đọc số liệu của Recorder mỗi 200 ms khi đang ghi.
class RecordTab : public QWidget
{
    Q_OBJECT
public:
    explicit RecordTab(QWidget *parent = nullptr);

    void setRecorder(Recorder *recorder) { m_recorder = recorder; }
    void setRecording(bool recording);

signals:
    void recordToggled(bool start);

private:
    void refreshStats();

    Recorder *m_recorder = nullptr;
    QPushButton *m_recordBtn = nullptr;
    QWidget *m_recordInfo = nullptr;
    QLabel *m_fileValue = nullptr;
    QLabel *m_timeValue = nullptr;
    QLabel *m_packetsValue = nullptr;
    QLabel *m_sizeValue = nullptr;
    QTimer *m_statsTimer = nullptr;
    bool m_recording = false;
};
