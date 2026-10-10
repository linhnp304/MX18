#include "ui/RecordTab.h"

#include "core/AppPaths.h"
#include "record/Recorder.h"
#include "record/Replayer.h"
#include "ui/ControlWidgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <iterator>

namespace {

constexpr int kStatsMs = 200;
// Thanh trượt đi theo bước 100 ms: file 2 giờ là 72 000 bước, vẫn trong int.
constexpr int kSliderUnitMs = 100;
constexpr double kRates[] = {0.5, 1.0, 2.0, 4.0, 8.0};
constexpr int kDefaultRate = 1;

QLabel *infoRow(QGridLayout *grid, int row, const QString &title, QWidget *parent)
{
    auto *name = new QLabel(title, parent);
    auto *value = new QLabel(parent);
    value->setStyleSheet(QStringLiteral("color:#7fc4ff;font-weight:bold;"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(name, row, 0);
    grid->addWidget(value, row, 1);
    return value;
}

QGridLayout *infoGrid(QWidget *box)
{
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    grid->setColumnStretch(1, 1);
    return grid;
}

QString hms(qint64 ms)
{
    const qint64 s = qMax<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QLatin1Char('0'))
        .arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString megabytes(quint64 bytes)
{
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

QString stamp(const QDateTime &t)
{
    return t.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));
}

void styleToggle(QPushButton *btn, bool running)
{
    // Màu chữ đặt riêng thì phải kèm luật :disabled, nếu không nút bị khoá vẫn
    // sáng như bấm được.
    btn->setStyleSheet(QStringLiteral("QPushButton { color:%1; font-weight:bold; }"
                                      "QPushButton:disabled { color:#5d666f; }")
                           .arg(running ? QStringLiteral("#ff6b6b") : QStringLiteral("#7ee08a")));
}

} // namespace

RecordTab::RecordTab(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *page = new QWidget(scroll);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    // --- nhóm "Ghi lưu"
    auto *recordGroup = new QGroupBox(QStringLiteral("Ghi lưu"), page);
    auto *rl = new QVBoxLayout(recordGroup);
    rl->setContentsMargins(8, 6, 8, 8);
    rl->setSpacing(6);
    m_recordBtn = new QPushButton(recordGroup);
    m_recordBtn->setMinimumHeight(28);
    rl->addWidget(m_recordBtn);

    m_recordInfo = new QWidget(recordGroup);
    QGridLayout *grid = infoGrid(m_recordInfo);
    m_fileValue = infoRow(grid, 0, QStringLiteral("Tên file:"), m_recordInfo);
    m_timeValue = infoRow(grid, 1, QStringLiteral("Thời gian ghi:"), m_recordInfo);
    m_packetsValue = infoRow(grid, 2, QStringLiteral("Tổng số gói tin:"), m_recordInfo);
    m_sizeValue = infoRow(grid, 3, QStringLiteral("Dung lượng file:"), m_recordInfo);
    rl->addWidget(m_recordInfo);
    lay->addWidget(recordGroup);

    lay->addWidget(buildReplayGroup(page));
    lay->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll, 1);

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(kStatsMs);
    connect(m_statsTimer, &QTimer::timeout, this, &RecordTab::refreshStats);
    connect(m_recordBtn, &QPushButton::clicked, this, [this] { emit recordToggled(!m_recording); });

    m_replayTimer = new QTimer(this);
    m_replayTimer->setInterval(kStatsMs);
    connect(m_replayTimer, &QTimer::timeout, this, &RecordTab::refreshReplay);

    setRecording(false);
    setReplaying(false);
}

QWidget *RecordTab::buildReplayGroup(QWidget *parent)
{
    auto *group = new QGroupBox(QStringLiteral("Phát lại"), parent);
    auto *pl = new QVBoxLayout(group);
    pl->setContentsMargins(8, 6, 8, 8);
    pl->setSpacing(6);

    // Panel 2 hẹp nên nhãn nằm trên ComboBox chứ không cùng hàng.
    pl->addWidget(new QLabel(QStringLiteral("Danh sách ghi lưu:"), group));
    m_fileCombo = new QComboBox(group);
    m_fileCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pl->addWidget(m_fileCombo);

    m_fileInfo = new QWidget(group);
    QGridLayout *fg = infoGrid(m_fileInfo);
    m_startValue = infoRow(fg, 0, QStringLiteral("Thời gian bắt đầu:"), m_fileInfo);
    m_endValue = infoRow(fg, 1, QStringLiteral("Thời gian kết thúc:"), m_fileInfo);
    m_durationValue = infoRow(fg, 2, QStringLiteral("Tổng thời gian ghi:"), m_fileInfo);
    m_fileSizeValue = infoRow(fg, 3, QStringLiteral("Dung lượng file:"), m_fileInfo);
    m_filePacketsValue = infoRow(fg, 4, QStringLiteral("Tổng số gói tin:"), m_fileInfo);
    pl->addWidget(m_fileInfo);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    m_replayBtn = new QPushButton(group);
    m_replayBtn->setMinimumHeight(28);
    m_pauseBtn = new QPushButton(QStringLiteral("Tạm dừng"), group);
    m_pauseBtn->setMinimumHeight(28);
    buttons->addWidget(m_replayBtn, 1);
    buttons->addWidget(m_pauseBtn, 1);
    pl->addLayout(buttons);

    m_slider = new QSlider(Qt::Horizontal, group);
    m_slider->setObjectName(QStringLiteral("replaySlider"));
    m_slider->setSingleStep(1000 / kSliderUnitMs);
    m_slider->setPageStep(60000 / kSliderUnitMs);
    m_slider->setToolTip(QStringLiteral("Bấm giữ và kéo tới thời điểm cần xem lại"));
    pl->addWidget(m_slider);
    m_positionLabel = new QLabel(group);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet(QStringLiteral("color:#8a95a1;"));
    pl->addWidget(m_positionLabel);

    QStringList labels;
    QVector<quint32> values;
    for (int i = 0; i < int(std::size(kRates)); ++i) {
        labels << QStringLiteral("%1x").arg(kRates[i]);
        values << quint32(i);
    }
    m_speed = new RadioRow(QStringLiteral("Tốc độ:"), labels, values, int(labels.size()), group);
    m_speed->setValue(kDefaultRate);
    pl->addWidget(m_speed);

    m_sendScn = new QCheckBox(QStringLiteral("Gửi thông tin phát lại đến X18-SCN"), group);
    m_sendVq = new QCheckBox(QStringLiteral("Gửi thông tin phát lại đến SCH-VQ"), group);
    m_sendScn->setToolTip(QStringLiteral("Điểm dấu MH phát lại gửi sang PC qua dòng X18-SCN-S (giờ hiện tại)"));
    m_sendVq->setToolTip(QStringLiteral("Quỹ đạo, điểm dấu MH, North marker / Sector crossing gửi qua dòng "
                                        "SCH-VQ (giờ hiện tại)"));
    pl->addWidget(m_sendScn);
    pl->addWidget(m_sendVq);

    m_replayInfo = new QWidget(group);
    QGridLayout *rg = infoGrid(m_replayInfo);
    m_replayTime = infoRow(rg, 0, QStringLiteral("Thời gian:"), m_replayInfo);
    m_replayTotal = infoRow(rg, 1, QStringLiteral("Tổng số gói tin:"), m_replayInfo);
    m_replayVideoR = infoRow(rg, 2, QStringLiteral("Số gói VIDEO_R:"), m_replayInfo);
    m_replayVideoI = infoRow(rg, 3, QStringLiteral("Số gói VIDEO_I:"), m_replayInfo);
    m_replayPlot = infoRow(rg, 4, QStringLiteral("Số gói điểm dấu MH:"), m_replayInfo);
    m_replayTrack = infoRow(rg, 5, QStringLiteral("Số gói quỹ đạo:"), m_replayInfo);
    m_replayTime->setToolTip(QStringLiteral("Giờ lúc ghi của thời điểm đang phát"));
    m_replayPlot->setToolTip(QStringLiteral("Gói PLOT nhận về"));
    m_replayTrack->setToolTip(QStringLiteral("Gói quỹ đạo nhận về (datagram X18-VQ)"));
    pl->addWidget(m_replayInfo);

    connect(m_fileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RecordTab::showFileInfo);
    connect(m_replayBtn, &QPushButton::clicked, this, [this] { emit replayToggled(!m_replaying); });
    connect(m_pauseBtn, &QPushButton::clicked, this, [this] {
        setPaused(!m_paused);
        emit pauseToggled(m_paused);
    });
    // Kéo thì chỉ xem trước mốc thời gian, thả tay mới tua: mỗi lần tua xoá quỹ
    // đạo / video và dựng lại từ khối gần nhất.
    connect(m_slider, &QSlider::sliderMoved, this,
            [this](int v) { updatePositionLabel(qint64(v) * kSliderUnitMs); });
    connect(m_slider, &QSlider::sliderReleased, this,
            [this] { emit seekRequested(qint64(m_slider->value()) * kSliderUnitMs); });
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        // Bấm trên rãnh hoặc phím mũi tên: tua ngay.
        if (m_settingSlider || m_slider->isSliderDown() || !m_replaying)
            return;
        emit seekRequested(qint64(v) * kSliderUnitMs);
    });
    connect(m_speed, &RadioRow::valueChanged, this, [this] { emit rateChanged(rate()); });
    connect(m_sendScn, &QCheckBox::toggled, this, &RecordTab::sendOptionsChanged);
    connect(m_sendVq, &QCheckBox::toggled, this, &RecordTab::sendOptionsChanged);
    return group;
}

void RecordTab::setRecording(bool recording)
{
    m_recording = recording;
    m_recordBtn->setText(recording ? QStringLiteral("Dừng ghi lưu") : QStringLiteral("Bắt đầu ghi lưu"));
    styleToggle(m_recordBtn, recording);
    // Nhãn chỉ hiện khi đang ghi (step-07); dừng thì số liệu file vừa đóng đã
    // có trong "Thông báo hệ thống".
    m_recordInfo->setVisible(recording);
    if (recording) {
        refreshStats();
        m_statsTimer->start();
    } else {
        m_statsTimer->stop();
    }
}

void RecordTab::refreshStats()
{
    if (!m_recorder)
        return;
    const RecordStats st = m_recorder->stats();
    m_fileValue->setText(QFileInfo(st.fileName).fileName());
    m_fileValue->setToolTip(QStringLiteral("./records/%1").arg(st.fileName));
    m_timeValue->setText(hms(st.elapsedMs));
    m_packetsValue->setText(QString::number(st.packets));
    m_sizeValue->setText(megabytes(st.bytes));
}

// --------------------------------------------------------------- phát lại

QStringList RecordTab::refreshRecordList()
{
    const QString keep = selectedName();
    QStringList errors;
    m_files = RecordFormat::scanRecords(AppPaths::recordsDir(), &errors);

    m_fileCombo->blockSignals(true);
    m_fileCombo->clear();
    int current = 0;
    for (int i = 0; i < m_files.size(); ++i) {
        const RecordFormat::Summary &s = m_files.at(i);
        m_fileCombo->addItem(s.title);
        m_fileCombo->setItemData(i, QStringLiteral("./records/%1").arg(s.relName), Qt::ToolTipRole);
        if (s.relName == keep)
            current = i;
    }
    if (m_files.isEmpty())
        m_fileCombo->addItem(QStringLiteral("(chưa có file ghi lưu)"));
    m_fileCombo->setCurrentIndex(current);
    m_fileCombo->blockSignals(false);
    showFileInfo();
    if (!m_replaying)
        m_replayBtn->setEnabled(!m_files.isEmpty());
    return errors;
}

QString RecordTab::selectedFile() const
{
    const int i = m_fileCombo->currentIndex();
    return (i >= 0 && i < m_files.size()) ? m_files.at(i).path : QString();
}

QString RecordTab::selectedName() const
{
    const int i = m_fileCombo->currentIndex();
    return (i >= 0 && i < m_files.size()) ? m_files.at(i).relName : QString();
}

double RecordTab::rate() const
{
    const quint32 i = m_speed->value();
    return i < std::size(kRates) ? kRates[i] : 1.0;
}

bool RecordTab::sendToScn() const
{
    return m_sendScn->isChecked();
}

bool RecordTab::sendToVq() const
{
    return m_sendVq->isChecked();
}

void RecordTab::showFileInfo()
{
    const int i = m_fileCombo->currentIndex();
    const bool has = (i >= 0 && i < m_files.size());
    m_fileInfo->setVisible(has);
    if (!has) {
        m_durationMs = 0;
        updatePositionLabel(0);
        return;
    }
    const RecordFormat::Summary &s = m_files.at(i);
    m_startValue->setText(stamp(s.start));
    m_endValue->setText(stamp(s.end));
    m_durationValue->setText(hms(s.durationMs));
    m_fileSizeValue->setText(megabytes(s.bytes));
    m_filePacketsValue->setText(QString::number(s.packets));
    // File ghi dở (mất điện) không có dấu đóng: số liệu là đếm lại từ các khối.
    m_endValue->setToolTip(s.closed ? QString()
                                    : QStringLiteral("File chưa được đóng đàng hoàng; số liệu đếm lại từ dữ liệu"));
    if (!m_replaying) {
        m_durationMs = s.durationMs;
        updatePositionLabel(0);
    }
}

void RecordTab::setReplaying(bool replaying)
{
    m_replaying = replaying;
    m_replayBtn->setText(replaying ? QStringLiteral("Dừng phát lại") : QStringLiteral("Phát lại"));
    styleToggle(m_replayBtn, replaying);
    m_replayBtn->setEnabled(replaying || !m_files.isEmpty());
    m_pauseBtn->setEnabled(replaying);
    m_slider->setEnabled(replaying);
    m_fileCombo->setEnabled(!replaying);
    // Đang phát lại thì cổng nhận đã đóng, ghi lưu lúc này chỉ ra file rỗng.
    m_recordBtn->setEnabled(!replaying);
    m_replayInfo->setVisible(replaying);
    setPaused(false);

    m_settingSlider = true;
    if (replaying && m_replayer) {
        m_durationMs = m_replayer->stats().durationMs;
        m_slider->setRange(0, int((m_durationMs + kSliderUnitMs - 1) / kSliderUnitMs));
        m_slider->setValue(0);
        refreshReplay();
        m_replayTimer->start();
    } else {
        m_replayTimer->stop();
        m_slider->setRange(0, 0);
        showFileInfo();
    }
    m_settingSlider = false;
}

void RecordTab::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseBtn->setText(paused ? QStringLiteral("Tiếp tục") : QStringLiteral("Tạm dừng"));
}

void RecordTab::refreshReplay()
{
    if (!m_replayer)
        return;
    const ReplayStats st = m_replayer->stats();
    if (!m_slider->isSliderDown()) {
        m_settingSlider = true;
        m_slider->setValue(int(st.posMs / kSliderUnitMs));
        m_settingSlider = false;
        updatePositionLabel(st.posMs);
    }
    m_replayTime->setText(stamp(QDateTime::fromMSecsSinceEpoch(st.wallMs)));
    m_replayTotal->setText(QString::number(st.total));
    m_replayVideoR->setText(QString::number(st.videoR));
    m_replayVideoI->setText(QString::number(st.videoI));
    m_replayPlot->setText(QString::number(st.plot));
    m_replayTrack->setText(QString::number(st.track));
}

void RecordTab::updatePositionLabel(qint64 posMs)
{
    m_positionLabel->setText(QStringLiteral("%1 / %2").arg(hms(posMs), hms(m_durationMs)));
}
