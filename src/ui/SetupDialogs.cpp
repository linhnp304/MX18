#include "ui/SetupDialogs.h"

#include "ui/Theme.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
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

const char *kColorCaptions[] = {
    "Đường quét và đường chia độ",
    "Vết lịch sử quỹ đạo",
    "Quỹ đạo",
    "Điểm dấu MH",
};

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
    for (int i = 0; i < kColorCount; ++i) {
        m_swatch[i] = new QPushButton(colorPage);
        m_swatch[i]->setFixedSize(84, 22);
        m_swatch[i]->setCursor(Qt::PointingHandCursor);
        connect(m_swatch[i], &QPushButton::clicked, this, [this, i] {
            const QColor c = QColorDialog::getColor(m_colors[i], this,
                                                    QString::fromUtf8(kColorCaptions[i]));
            if (c.isValid()) {
                m_colors[i] = c;
                updateSwatch(i);
            }
        });
        colorLay->addRow(QString::fromUtf8(kColorCaptions[i]) + QLatin1Char(':'), m_swatch[i]);
    }
    auto *note = new QLabel(QStringLiteral("(các đối tượng khác sẽ bổ sung ở giai đoạn sau)"),
                            colorPage);
    note->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    colorLay->addRow(note);
    tabs->addTab(colorPage, QStringLiteral("Màu sắc"));

    // ---- tab Thiết lập khác
    auto *otherPage = new QWidget(tabs);
    auto *otherLay = new QVBoxLayout(otherPage);
    otherLay->setContentsMargins(10, 10, 10, 10);
    otherLay->setSpacing(8);

    auto *holdRow = new QHBoxLayout;
    holdRow->addWidget(new QLabel(QStringLiteral("Thời gian hiển thị điểm dấu MH:"), otherPage));
    m_plotHold = new QSpinBox(otherPage);
    m_plotHold->setRange(1, 600);
    m_plotHold->setSuffix(QStringLiteral(" giây"));
    holdRow->addWidget(m_plotHold);
    holdRow->addStretch(1);
    otherLay->addLayout(holdRow);

    otherLay->addWidget(sizeGroup(QStringLiteral("Kích thước quỹ đạo"), m_trackSize, otherPage));
    otherLay->addWidget(sizeGroup(QStringLiteral("Kích thước điểm dấu MH"), m_plotSize, otherPage));

    auto *note2 = new QLabel(QStringLiteral("(các tham số khác sẽ bổ sung ở giai đoạn sau)"),
                             otherPage);
    note2->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    otherLay->addWidget(note2);
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

    connect(restoreBtn, &QPushButton::clicked, this, &ColorSetupDialog::restoreDefaults);
    connect(applyBtn, &QPushButton::clicked, this, [this] {
        applyToSettings();
        emit applied();
    });

    loadFromSettings();
}

void ColorSetupDialog::updateSwatch(int index)
{
    m_swatch[index]->setStyleSheet(
        QStringLiteral("background:%1;border:1px solid #6a747e;").arg(m_colors[index].name()));
    m_swatch[index]->setText(m_colors[index].name().toUpper());
    const bool dark = m_colors[index].lightness() < 130;
    m_swatch[index]->setStyleSheet(
        QStringLiteral("background:%1;color:%2;border:1px solid #6a747e;")
            .arg(m_colors[index].name(), dark ? QStringLiteral("#ffffff") : QStringLiteral("#101010")));
}

void ColorSetupDialog::loadFromSettings()
{
    const Setups &s = Settings::instance().setups();
    m_colors[0] = s.colors.grid;
    m_colors[1] = s.colors.trackTrail;
    m_colors[2] = s.colors.track;
    m_colors[3] = s.colors.plot;
    for (int i = 0; i < kColorCount; ++i)
        updateSwatch(i);

    m_plotHold->setValue(s.plotHoldSec);
    for (int i = 0; i < 5; ++i) {
        m_trackSize[i]->setChecked(kSizeOptions[i] == s.trackSizePct);
        m_plotSize[i]->setChecked(kSizeOptions[i] == s.plotSizePct);
    }
}

void ColorSetupDialog::applyToSettings()
{
    Setups &s = Settings::instance().setups();
    s.colors.grid = m_colors[0];
    s.colors.trackTrail = m_colors[1];
    s.colors.track = m_colors[2];
    s.colors.plot = m_colors[3];
    s.plotHoldSec = m_plotHold->value();
    for (int i = 0; i < 5; ++i) {
        if (m_trackSize[i]->isChecked())
            s.trackSizePct = kSizeOptions[i];
        if (m_plotSize[i]->isChecked())
            s.plotSizePct = kSizeOptions[i];
    }
    Settings::instance().saveSetups();
}

void ColorSetupDialog::restoreDefaults()
{
    const Setups def;
    m_colors[0] = def.colors.grid;
    m_colors[1] = def.colors.trackTrail;
    m_colors[2] = def.colors.track;
    m_colors[3] = def.colors.plot;
    for (int i = 0; i < kColorCount; ++i)
        updateSwatch(i);
    m_plotHold->setValue(def.plotHoldSec);
    for (int i = 0; i < 5; ++i) {
        m_trackSize[i]->setChecked(kSizeOptions[i] == def.trackSizePct);
        m_plotSize[i]->setChecked(kSizeOptions[i] == def.plotSizePct);
    }
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
