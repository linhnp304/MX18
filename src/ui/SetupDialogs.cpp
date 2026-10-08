#include "ui/SetupDialogs.h"

#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

const int kSizeOptions[5] = {50, 75, 100, 150, 200};

QGroupBox *sizeGroup(const QString &title, QRadioButton **out, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    auto *lay = new QHBoxLayout(g);
    lay->setContentsMargins(8, 4, 8, 6);
    auto *bg = new QButtonGroup(g);
    for (int i = 0; i < 5; ++i) {
        auto *rb = new QRadioButton(QStringLiteral("%1%").arg(kSizeOptions[i]), g);
        bg->addButton(rb, i);
        lay->addWidget(rb);
        out[i] = rb;
    }
    lay->addStretch(1);
    return g;
}

// Ô nhập chỉ chứa số; đơn vị là nhãn riêng bên phải (step-06 mục 11).
QSpinBox *secondsBox(int min, int max, QWidget *parent)
{
    auto *box = new QSpinBox(parent);
    box->setRange(min, max);
    box->setMinimumWidth(70);
    box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return box;
}

QDoubleSpinBox *windowBox(double max, QWidget *parent)
{
    auto *box = new QDoubleSpinBox(parent);
    box->setDecimals(3);
    box->setRange(0.1, max);
    box->setSingleStep(0.5);
    box->setMinimumWidth(80);
    box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return box;
}

} // namespace

ColorSetupDialog::ColorSetupDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle(QStringLiteral("Màu sắc và thiết lập khác"));
    // Yêu cầu: cửa sổ này luôn nổi trên cùng khi đang mở.
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

    auto *lay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    // ---- tab Màu sắc
    auto *colorPage = new QWidget(tabs);
    auto *colorLay = new QFormLayout(colorPage);
    colorLay->setContentsMargins(10, 10, 10, 10);
    const QString captions[kColorCount] = {
        QStringLiteral("Đường quét và đường chia độ"),
        QStringLiteral("Vết lịch sử quỹ đạo"),
        QStringLiteral("Quỹ đạo"),
        QStringLiteral("Quỹ đạo có nhận dạng MH"),
        QStringLiteral("Lý lịch quỹ đạo"),
        QStringLiteral("Điểm dấu MH"),
        QStringLiteral("Tia báo động (màu 1)"),
        QStringLiteral("Tia báo động (màu 2)"),
    };
    for (int i = 0; i < kColorCount; ++i) {
        m_swatch[i] = new QPushButton(colorPage);
        m_swatch[i]->setFixedSize(84, 22);
        m_swatch[i]->setCursor(Qt::PointingHandCursor);
        m_swatch[i]->setToolTip(captions[i]);
        connect(m_swatch[i], &QPushButton::clicked, this, [this, i, captions] {
            const QColor c = QColorDialog::getColor(m_colors[i], this, captions[i]);
            if (c.isValid()) {
                m_colors[i] = c;
                updateSwatch(i);
            }
        });
    }
    for (int i = CGrid; i < CAlarm1; ++i)
        colorLay->addRow(captions[i] + QLatin1Char(':'), m_swatch[i]);
    // Tia báo động nhấp nháy giữa hai màu nên chọn cả hai trên một dòng.
    auto *pairLay = new QHBoxLayout;
    pairLay->setSpacing(6);
    pairLay->addWidget(m_swatch[CAlarm1]);
    pairLay->addWidget(m_swatch[CAlarm2]);
    pairLay->addStretch(1);
    colorLay->addRow(QStringLiteral("Tia báo động:"), pairLay);
    tabs->addTab(colorPage, QStringLiteral("Màu sắc"));

    // ---- tab Thiết lập khác
    auto *otherPage = new QWidget(tabs);
    auto *otherLay = new QVBoxLayout(otherPage);
    otherLay->setContentsMargins(10, 10, 10, 10);
    otherLay->setSpacing(8);

    auto *timeGrid = new QGridLayout;
    timeGrid->setHorizontalSpacing(6);
    m_plotHold = secondsBox(1, 60, otherPage);
    m_trackDrop = secondsBox(10, 600, otherPage);
    timeGrid->addWidget(new QLabel(QStringLiteral("Thời gian hiển thị điểm dấu MH:"), otherPage), 0, 0);
    timeGrid->addWidget(m_plotHold, 0, 1);
    timeGrid->addWidget(new QLabel(QStringLiteral("giây"), otherPage), 0, 2);
    timeGrid->addWidget(new QLabel(QStringLiteral("Thời gian xóa quỹ đạo khi không có cập nhật:"), otherPage), 1, 0);
    timeGrid->addWidget(m_trackDrop, 1, 1);
    timeGrid->addWidget(new QLabel(QStringLiteral("giây"), otherPage), 1, 2);
    timeGrid->setColumnStretch(3, 1);
    otherLay->addLayout(timeGrid);

    otherLay->addWidget(sizeGroup(QStringLiteral("Kích thước quỹ đạo"), m_trackSize, otherPage));
    otherLay->addWidget(sizeGroup(QStringLiteral("Kích thước điểm dấu MH"), m_plotSize, otherPage));

    m_mhTrackInit = new QCheckBox(QStringLiteral("Khởi tạo quỹ đạo từ điểm dấu MH"), otherPage);
    m_mhTrackInit->setToolTip(QStringLiteral(
        "Bỏ chọn: hợp nhất thông tin điểm dấu MH vào quỹ đạo nhận từ X18-VQ.\n"
        "Chọn: không hợp nhất, khởi tạo và bám quỹ đạo từ điểm dấu MH."));
    otherLay->addWidget(m_mhTrackInit);

    auto *mergeGroup = new QGroupBox(QStringLiteral("Kích thước cửa sổ hợp nhất"), otherPage);
    // Theme tô nền tối cho mọi QWidget: nhãn trong nhóm phải trong suốt để
    // cùng màu nền nhóm.
    mergeGroup->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
    auto *mergeLay = new QGridLayout(mergeGroup);
    mergeLay->setContentsMargins(8, 4, 8, 6);
    mergeLay->setHorizontalSpacing(6);
    m_mergeAzimuth = windowBox(30.0, mergeGroup);
    m_mergeRange = windowBox(50.0, mergeGroup);
    mergeLay->addWidget(new QLabel(QStringLiteral("Phương vị (độ):"), mergeGroup), 0, 0);
    mergeLay->addWidget(m_mergeAzimuth, 0, 1);
    mergeLay->addWidget(new QLabel(QStringLiteral("Cự ly (km):"), mergeGroup), 1, 0);
    mergeLay->addWidget(m_mergeRange, 1, 1);
    mergeLay->setColumnStretch(2, 1);
    otherLay->addWidget(mergeGroup);
    otherLay->addStretch(1);
    tabs->addTab(otherPage, QStringLiteral("Thiết lập khác"));

    lay->addWidget(tabs);

    auto *btnRow = new QHBoxLayout;
    auto *restoreBtn = new QPushButton(QStringLiteral("Khôi phục mặc định"), this);
    auto *applyBtn = new QPushButton(QStringLiteral("Áp dụng"), this);
    btnRow->addWidget(restoreBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(applyBtn);
    lay->addLayout(btnRow);

    connect(restoreBtn, &QPushButton::clicked, this, [this] { loadValues(Setups()); });
    connect(applyBtn, &QPushButton::clicked, this, [this] {
        applyToSettings();
        emit applied();
    });

    loadFromSettings();
}

QColor &ColorSetupDialog::colorRef(DisplayColors &c, int index) const
{
    switch (index) {
    case CGrid:    return c.grid;
    case CTrail:   return c.trackTrail;
    case CTrack:   return c.track;
    case CTrackMh: return c.trackMh;
    case CProfile: return c.trackProfile;
    case CPlot:    return c.plot;
    case CAlarm1:  return c.alarm1;
    default:       return c.alarm2;
    }
}

void ColorSetupDialog::updateSwatch(int index)
{
    m_swatch[index]->setText(m_colors[index].name().toUpper());
    const bool dark = m_colors[index].lightness() < 130;
    m_swatch[index]->setStyleSheet(
        QStringLiteral("background:%1;color:%2;border:1px solid #6a747e;")
            .arg(m_colors[index].name(), dark ? QStringLiteral("#ffffff") : QStringLiteral("#101010")));
}

void ColorSetupDialog::loadFromSettings()
{
    loadValues(Settings::instance().setups());
}

void ColorSetupDialog::loadValues(const Setups &s)
{
    DisplayColors colors = s.colors;
    for (int i = 0; i < kColorCount; ++i) {
        m_colors[i] = colorRef(colors, i);
        updateSwatch(i);
    }
    m_plotHold->setValue(s.plotHoldSec);
    m_trackDrop->setValue(s.trackDropSec);
    for (int i = 0; i < 5; ++i) {
        m_trackSize[i]->setChecked(kSizeOptions[i] == s.trackSizePct);
        m_plotSize[i]->setChecked(kSizeOptions[i] == s.plotSizePct);
    }
    m_mhTrackInit->setChecked(s.mhTrackInit);
    m_mergeAzimuth->setValue(s.mergeAzimuthDeg);
    m_mergeRange->setValue(s.mergeRangeKm);
}

void ColorSetupDialog::applyToSettings()
{
    Setups &s = Settings::instance().setups();
    for (int i = 0; i < kColorCount; ++i)
        colorRef(s.colors, i) = m_colors[i];
    s.plotHoldSec = m_plotHold->value();
    s.trackDropSec = m_trackDrop->value();
    for (int i = 0; i < 5; ++i) {
        if (m_trackSize[i]->isChecked())
            s.trackSizePct = kSizeOptions[i];
        if (m_plotSize[i]->isChecked())
            s.plotSizePct = kSizeOptions[i];
    }
    s.mhTrackInit = m_mhTrackInit->isChecked();
    s.mergeAzimuthDeg = m_mergeAzimuth->value();
    s.mergeRangeKm = m_mergeRange->value();
    Settings::instance().saveSetups();
}

// ---------------------------------------------------------- mật khẩu kỹ sư

EngineerPasswordDialog::EngineerPasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Nhập mật khẩu kỹ sư"));
    setModal(true);

    auto *lay = new QVBoxLayout(this);
    auto *row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("Mật khẩu:"), this));
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setMinimumWidth(160);
    row->addWidget(m_password, 1);
    lay->addLayout(row);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Đồng ý"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Huỷ"));
    lay->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(box, &QDialogButtonBox::accepted, this, [this] {
        if (m_password->text() == Settings::instance().engineerPassword()) {
            accept();
        } else {
            QMessageBox::warning(this, QStringLiteral("Sai mật khẩu"),
                                 QStringLiteral("Mật khẩu kỹ sư không đúng."));
            m_password->selectAll();
            m_password->setFocus();
        }
    });
}
