#include "ui/SettingsTab.h"

#include "core/Settings.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

namespace {

QWidget *sliderRow(const QString &title, int lo, int hi, QSlider **slider, QLabel **value,
                   QWidget *parent)
{
    auto *box = new QWidget(parent);
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(0, 2, 0, 2);
    lay->setSpacing(2);

    auto *head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    auto *lbl = new QLabel(title, box);
    auto *val = new QLabel(box);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    val->setMinimumWidth(28);
    val->setStyleSheet(QStringLiteral("color:#7fc4ff;font-weight:bold;"));
    head->addWidget(lbl);
    head->addStretch(1);
    head->addWidget(val);

    auto *sl = new QSlider(Qt::Horizontal, box);
    sl->setRange(lo, hi);
    sl->setPageStep(1);

    lay->addLayout(head);
    lay->addWidget(sl);

    *slider = sl;
    *value = val;
    return box;
}

QGroupBox *radioGroup(const QString &title, const QStringList &labels, QRadioButton **out,
                      QWidget *parent)
{
    auto *grp = new QGroupBox(title, parent);
    auto *lay = new QHBoxLayout(grp);
    lay->setContentsMargins(8, 4, 8, 6);
    lay->setSpacing(6);
    auto *bg = new QButtonGroup(grp);
    for (int i = 0; i < labels.size(); ++i) {
        auto *rb = new QRadioButton(labels.at(i), grp);
        bg->addButton(rb, i);
        lay->addWidget(rb);
        out[i] = rb;
    }
    lay->addStretch(1);
    return grp;
}

} // namespace

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    loadFromSettings();
}

void SettingsTab::buildUi()
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

    m_showMap = new QCheckBox(QStringLiteral("Hiện nền bản đồ số"), page);
    m_showAirRoutes = new QCheckBox(QStringLiteral("Hiện đường bay dân dụng"), page);
    m_showAirports = new QCheckBox(QStringLiteral("Hiện sân bay"), page);
    lay->addWidget(m_showMap);
    lay->addWidget(m_showAirRoutes);
    lay->addWidget(m_showAirports);

    lay->addWidget(sliderRow(QStringLiteral("Độ sáng bản đồ"), 1, 10,
                             &m_brightness, &m_brightnessValue, page));
    lay->addWidget(sliderRow(QStringLiteral("Tốc độ mờ video"), 0, 10,
                             &m_videoFade, &m_videoFadeValue, page));
    lay->addWidget(sliderRow(QStringLiteral("Số vết lịch sử quỹ đạo"), 0, 100,
                             &m_trackHistory, &m_trackHistoryValue, page));

    QRadioButton *trail[2] = {nullptr, nullptr};
    lay->addWidget(radioGroup(QStringLiteral("Dạng vết quỹ đạo"),
                              {QStringLiteral("Điểm"), QStringLiteral("Đường")}, trail, page));
    m_trailDot = trail[0];
    m_trailLine = trail[1];

    m_showTrackProfile = new QCheckBox(QStringLiteral("Hiện lí lịch quỹ đạo"), page);
    m_showPlotInfo = new QCheckBox(QStringLiteral("Hiện thông tin điểm dấu MH"), page);
    lay->addWidget(m_showTrackProfile);
    lay->addWidget(m_showPlotInfo);

    lay->addWidget(radioGroup(QStringLiteral("Vòng cự ly"),
                              {QStringLiteral("50 km"), QStringLiteral("10 km"),
                               QStringLiteral("5 km"), QStringLiteral("Tắt")},
                              m_ring, page));
    lay->addWidget(radioGroup(QStringLiteral("Đường chia độ"),
                              {QStringLiteral("30 độ"), QStringLiteral("10 độ"),
                               QStringLiteral("5 độ"), QStringLiteral("Tắt")},
                              m_azimuth, page));

    m_colorSetupBtn = new QPushButton(QStringLiteral("Màu sắc và thiết lập khác"), page);
    lay->addWidget(m_colorSetupBtn);

    lay->addSpacing(6);
    m_connectBtn = new QPushButton(QStringLiteral("Kết nối"), page);
    m_connectBtn->setMinimumHeight(28);
    m_exitBtn = new QPushButton(QStringLiteral("Thoát phần mềm"), page);
    lay->addWidget(m_connectBtn);
    lay->addWidget(m_exitBtn);
    lay->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll);

    // --- nối tín hiệu: mọi thay đổi đều lưu ngay và vẽ lại bản đồ
    const auto onChange = [this] {
        if (m_loading)
            return;
        pushToSettings();
        emit displayChanged();
    };
    for (QCheckBox *cb : {m_showMap, m_showAirRoutes, m_showAirports,
                          m_showTrackProfile, m_showPlotInfo})
        connect(cb, &QCheckBox::toggled, this, onChange);
    for (QSlider *sl : {m_brightness, m_videoFade, m_trackHistory})
        connect(sl, &QSlider::valueChanged, this, onChange);
    for (QRadioButton *rb : {m_trailDot, m_trailLine})
        connect(rb, &QRadioButton::toggled, this, onChange);
    for (int i = 0; i < 4; ++i) {
        connect(m_ring[i], &QRadioButton::toggled, this, onChange);
        connect(m_azimuth[i], &QRadioButton::toggled, this, onChange);
    }

    connect(m_brightness, &QSlider::valueChanged, this,
            [this](int v) { m_brightnessValue->setText(QString::number(v)); });
    connect(m_videoFade, &QSlider::valueChanged, this,
            [this](int v) { m_videoFadeValue->setText(QString::number(v)); });
    connect(m_trackHistory, &QSlider::valueChanged, this,
            [this](int v) { m_trackHistoryValue->setText(QString::number(v)); });

    connect(m_colorSetupBtn, &QPushButton::clicked, this, &SettingsTab::colorSetupRequested);
    connect(m_connectBtn, &QPushButton::clicked, this,
            [this] { emit connectToggled(!m_connected); });
    connect(m_exitBtn, &QPushButton::clicked, this, &SettingsTab::exitRequested);
}

void SettingsTab::loadFromSettings()
{
    const Setups &s = Settings::instance().setups();
    m_loading = true;

    m_showMap->setChecked(s.showMap);
    m_showAirRoutes->setChecked(s.showAirRoutes);
    m_showAirports->setChecked(s.showAirports);
    m_brightness->setValue(s.brightness);
    m_videoFade->setValue(s.videoFade);
    m_trackHistory->setValue(s.trackHistory);
    (s.trailStyle == 1 ? m_trailLine : m_trailDot)->setChecked(true);
    m_showTrackProfile->setChecked(s.showTrackProfile);
    m_showPlotInfo->setChecked(s.showPlotInfo);
    m_ring[qBound(0, s.rangeRingMode, 3)]->setChecked(true);
    m_azimuth[qBound(0, s.azimuthMode, 3)]->setChecked(true);

    m_brightnessValue->setText(QString::number(s.brightness));
    m_videoFadeValue->setText(QString::number(s.videoFade));
    m_trackHistoryValue->setText(QString::number(s.trackHistory));

    m_loading = false;
}

void SettingsTab::pushToSettings()
{
    Setups &s = Settings::instance().setups();
    s.showMap = m_showMap->isChecked();
    s.showAirRoutes = m_showAirRoutes->isChecked();
    s.showAirports = m_showAirports->isChecked();
    s.brightness = m_brightness->value();
    s.videoFade = m_videoFade->value();
    s.trackHistory = m_trackHistory->value();
    s.trailStyle = m_trailLine->isChecked() ? 1 : 0;
    s.showTrackProfile = m_showTrackProfile->isChecked();
    s.showPlotInfo = m_showPlotInfo->isChecked();
    for (int i = 0; i < 4; ++i) {
        if (m_ring[i]->isChecked())
            s.rangeRingMode = i;
        if (m_azimuth[i]->isChecked())
            s.azimuthMode = i;
    }
    Settings::instance().saveSetups();
}

void SettingsTab::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? QStringLiteral("Dừng kết nối")
                                    : QStringLiteral("Kết nối"));
    m_connectBtn->setStyleSheet(connected
        ? QStringLiteral("color:#ffd24d;font-weight:bold;")
        : QStringLiteral("color:#7ee08a;font-weight:bold;"));
    // Đang kết nối thì không cho thoát: phải dừng thu phát trước.
    m_exitBtn->setEnabled(!connected);
}
