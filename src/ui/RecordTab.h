#pragma once

#include "record/RecordFormat.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class RadioRow;
class Recorder;
class Replayer;

// Tab "Ghi lưu" của panel 2 (step-07): nhóm "Ghi lưu" và nhóm "Phát lại".
// Bắt đầu / dừng do MainWindow quyết (đọc records.json, kiểm điều kiện phát lại,
// báo thông báo hệ thống); tab chỉ phát yêu cầu và tự đọc số liệu của Recorder /
// Replayer mỗi 200 ms khi đang ghi / đang phát.
class RecordTab : public QWidget
{
    Q_OBJECT
public:
    explicit RecordTab(QWidget *parent = nullptr);

    void setRecorder(Recorder *recorder) { m_recorder = recorder; }
    void setReplayer(Replayer *replayer) { m_replayer = replayer; }
    void setRecording(bool recording);
    void setReplaying(bool replaying);
    // Hết file: nút về "Tiếp tục" (bấm thì phát lại từ đầu).
    void setPaused(bool paused);

    // Quét lại ./records, giữ lựa chọn cũ nếu file còn. Trả về các dòng lỗi.
    QStringList refreshRecordList();
    // Đường dẫn đầy đủ của file đang chọn, rỗng khi danh sách trống.
    QString selectedFile() const;
    QString selectedName() const;
    double rate() const;
    bool sendToScn() const;
    bool sendToVq() const;

signals:
    void recordToggled(bool start);
    void replayToggled(bool start);
    void pauseToggled(bool paused);
    void seekRequested(qint64 ms);
    void rateChanged(double rate);
    void sendOptionsChanged();

private:
    QWidget *buildReplayGroup(QWidget *parent);
    void refreshStats();
    void refreshReplay();
    void showFileInfo();
    void updatePositionLabel(qint64 posMs);

    Recorder *m_recorder = nullptr;
    QPushButton *m_recordBtn = nullptr;
    QWidget *m_recordInfo = nullptr;
    QLabel *m_fileValue = nullptr;
    QLabel *m_timeValue = nullptr;
    QLabel *m_packetsValue = nullptr;
    QLabel *m_sizeValue = nullptr;
    QTimer *m_statsTimer = nullptr;
    bool m_recording = false;

    Replayer *m_replayer = nullptr;
    QVector<RecordFormat::Summary> m_files;
    QComboBox *m_fileCombo = nullptr;
    QWidget *m_fileInfo = nullptr;
    QLabel *m_startValue = nullptr;
    QLabel *m_endValue = nullptr;
    QLabel *m_durationValue = nullptr;
    QLabel *m_fileSizeValue = nullptr;
    QLabel *m_filePacketsValue = nullptr;
    QPushButton *m_replayBtn = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QSlider *m_slider = nullptr;
    QLabel *m_positionLabel = nullptr;
    RadioRow *m_speed = nullptr;
    QCheckBox *m_sendScn = nullptr;
    QCheckBox *m_sendVq = nullptr;
    QWidget *m_replayInfo = nullptr;
    QLabel *m_replayTime = nullptr;
    QLabel *m_replayTotal = nullptr;
    QLabel *m_replayVideoR = nullptr;
    QLabel *m_replayVideoI = nullptr;
    QLabel *m_replayPlot = nullptr;
    QLabel *m_replayTrack = nullptr;
    QTimer *m_replayTimer = nullptr;
    qint64 m_durationMs = 0;
    bool m_replaying = false;
    bool m_paused = false;
    bool m_settingSlider = false;
};
