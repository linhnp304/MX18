#include "ui/RecordTab.h"

#include "record/Recorder.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kStatsMs = 200;

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

QString hms(qint64 ms)
{
    const qint64 s = qMax<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QLatin1Char('0'))
        .arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
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
    auto *grid = new QGridLayout(m_recordInfo);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    grid->setColumnStretch(1, 1);
    m_fileValue = infoRow(grid, 0, QStringLiteral("Tên file:"), m_recordInfo);
    m_timeValue = infoRow(grid, 1, QStringLiteral("Thời gian ghi:"), m_recordInfo);
    m_packetsValue = infoRow(grid, 2, QStringLiteral("Tổng số gói tin:"), m_recordInfo);
    m_sizeValue = infoRow(grid, 3, QStringLiteral("Dung lượng file:"), m_recordInfo);
    rl->addWidget(m_recordInfo);
    lay->addWidget(recordGroup);

    // --- nhóm "Phát lại" (giai đoạn 7 phiên 3)
    auto *replayGroup = new QGroupBox(QStringLiteral("Phát lại"), page);
    auto *pl = new QVBoxLayout(replayGroup);
    pl->setContentsMargins(8, 6, 8, 8);
    auto *todo = new QLabel(QStringLiteral("Chức năng phát lại đang làm."), replayGroup);
    todo->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    pl->addWidget(todo);
    lay->addWidget(replayGroup);
    lay->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll, 1);

    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(kStatsMs);
    connect(m_statsTimer, &QTimer::timeout, this, &RecordTab::refreshStats);
    connect(m_recordBtn, &QPushButton::clicked, this, [this] { emit recordToggled(!m_recording); });

    setRecording(false);
}

void RecordTab::setRecording(bool recording)
{
    m_recording = recording;
    m_recordBtn->setText(recording ? QStringLiteral("Dừng ghi lưu") : QStringLiteral("Bắt đầu ghi lưu"));
    m_recordBtn->setStyleSheet(recording ? QStringLiteral("color:#ff6b6b;font-weight:bold;")
                                         : QStringLiteral("color:#7ee08a;font-weight:bold;"));
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
    m_sizeValue->setText(QStringLiteral("%1 MB").arg(double(st.bytes) / (1024.0 * 1024.0), 0, 'f', 1));
}
