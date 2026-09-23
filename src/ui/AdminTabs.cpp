#include "ui/AdminTabs.h"

#include "proto/Dataframe.h"
#include "ui/ControlWidgets.h"

#include <QBrush>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

const QStringList kOnOff = {QStringLiteral("Tắt"), QStringLiteral("Bật")};
const QStringList kOffOn = {QStringLiteral("Off"), QStringLiteral("On")};
const QVector<quint32> k01 = {0, 1};

// Dải "toàn dải" của đặc tả: QSpinBox chỉ nhận int nên lấy hết dải int dương.
constexpr int kIntMax = 2147483647;

// MSVC không định nghĩa M_PI nếu thiếu _USE_MATH_DEFINES; khai báo thẳng cho gọn.
constexpr double kPi = 3.14159265358979323846;

QVector<quint32> upto(int n)
{
    QVector<quint32> v;
    for (int i = 0; i < n; ++i)
        v << quint32(i);
    return v;
}

// Nhãn serial: "—" khi chưa có gì để hiện.
QLabel *serialLabel(const QString &prefix, QWidget *parent)
{
    auto *l = new QLabel(prefix + QString::fromUtf8("—"), parent);
    l->setStyleSheet(QStringLiteral("color:#8a95a1;"));
    return l;
}

QVBoxLayout *groupLayout(QGroupBox *g)
{
    return qobject_cast<QVBoxLayout *>(g->layout());
}

QGroupBox *makeGroup(const QString &title, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    auto *lay = new QVBoxLayout(g);
    lay->setContentsMargins(10, 4, 10, 6);
    lay->setSpacing(1);
    return g;
}

// Trang cuộn được cho mỗi tab: các nhóm điều khiển cao hơn cửa sổ là chuyện
// thường ở độ phân giải 1280x1024.
QWidget *scrollPage(QWidget *tab, QVBoxLayout **innerLayout)
{
    auto *outer = new QVBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget(scroll);
    auto *lay = new QVBoxLayout(page);
    // Lề hẹp để cả tab "ADMIN" — tab dài nhất — lọt trong một màn hình 1080p.
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    scroll->setWidget(page);
    outer->addWidget(scroll, 1);
    *innerLayout = lay;
    return page;
}

QString round3(double v)
{
    return QString::number(v, 'f', 3);
}

} // namespace

// ------------------------------------------------------------ CommandBlock

CommandBlock::CommandBlock(quint32 category, quint32 backCategory, int count,
                           const quint32 *defaults, QObject *parent)
    : QObject(parent)
    , m_category(category)
    , m_backCategory(backCategory)
{
    m_fields.resize(count);
    m_back.fill(0, count);
    for (int i = 0; i < count; ++i)
        m_fields[i] = defaults[i];
}

FieldEditor *CommandBlock::bind(FieldEditor *editor, int field)
{
    editor->setRaw(m_fields.at(field));
    m_bound.append({editor, field});
    connect(editor, &FieldEditor::edited, this, [this, editor, field] {
        m_fields[field] = editor->raw();
        emit fieldEdited(field, m_fields.at(field));
        refresh();
    });
    return editor;
}

void CommandBlock::addLocked(QWidget *w)
{
    m_lockedWidgets.append(w);
    w->setEnabled(!m_locked);
}

void CommandBlock::setSendButton(QPushButton *btn, const QString &packetName)
{
    btn->setToolTip(QStringLiteral("Gửi gói %1").arg(packetName));
    addLocked(btn);
    connect(btn, &QPushButton::clicked, this, &CommandBlock::sendRequested);
}

void CommandBlock::setSerialLabels(QLabel *sent, QLabel *back)
{
    m_serialSent = sent;
    m_serialBack = back;
}

void CommandBlock::setField(int i, quint32 v)
{
    m_fields[i] = v;
    for (const Bound &b : std::as_const(m_bound)) {
        if (b.field == i)
            b.editor->setRaw(v);
    }
    refresh();
}

void CommandBlock::setLocked(bool locked)
{
    m_locked = locked;
    for (QWidget *w : std::as_const(m_lockedWidgets))
        w->setEnabled(!locked);
    refresh();
}

void CommandBlock::applyBack(const quint32 *fields, quint32 serial)
{
    for (int i = 0; i < m_back.size(); ++i)
        m_back[i] = fields[i];
    m_hasBack = true;
    m_backSerial = serial;
    m_hasBackSerial = true;
    refresh();
    emit backReceived();
    emit backSerialChanged();
}

void CommandBlock::noteSent(quint32 serial)
{
    if (m_serialSent)
        m_serialSent->setText(QStringLiteral("Gửi: %1").arg(serial));
    emit sent();
}

void CommandBlock::clearBack()
{
    m_hasBack = false;
    m_hasBackSerial = false;
    if (m_serialSent)
        m_serialSent->setText(QString::fromUtf8("Gửi: —"));
    refresh();
    emit backSerialChanged();
}

QString CommandBlock::backSerialText() const
{
    return m_hasBackSerial ? QStringLiteral("Phản hồi: %1").arg(m_backSerial)
                           : QString::fromUtf8("Phản hồi: —");
}

// Đang khoá thì các ô nhập chạy theo trạng thái thật của hệ thống MH; đang mở
// khoá thì giữ giá trị kỹ sư đang đặt và chỉ đánh dấu đỏ chỗ lệch.
void CommandBlock::refresh()
{
    if (m_serialBack)
        m_serialBack->setText(backSerialText());

    for (const Bound &b : std::as_const(m_bound)) {
        const quint32 back = m_hasBack ? m_back.at(b.field) : 0u;
        if (m_locked && m_hasBack) {
            m_fields[b.field] = back;
            b.editor->setRaw(back);
            b.editor->showFeedback(false, 0);
        } else {
            b.editor->showFeedback(m_hasBack && back != m_fields.at(b.field), back);
        }
    }
}

// --------------------------------------------------------------- lớp chung

// sizeHint() của tab chỉ tính đến sàn của QScrollArea nên bé hơn nội dung thật;
// muốn cửa sổ mở ra vừa khít thì phải lấy kích thước mong muốn của trang nằm
// trong vùng cuộn rồi cộng thêm các hàng đặt ngoài vùng cuộn.
QSize EngineerTab::contentSizeHint() const
{
    auto *scroll = findChild<QScrollArea *>();
    auto *lay = qobject_cast<QVBoxLayout *>(layout());
    if (!scroll || !scroll->widget() || !lay)
        return sizeHint();

    QSize s = scroll->widget()->sizeHint();
    s.rwidth() += 2 * scroll->frameWidth() + scroll->verticalScrollBar()->sizeHint().width();
    s.rheight() += 2 * scroll->frameWidth();

    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem *item = lay->itemAt(i);
        if (item->widget() == scroll)
            continue;
        s.rheight() += item->sizeHint().height() + lay->spacing();
        s.rwidth() = qMax(s.width(), item->sizeHint().width());
    }
    const QMargins m = lay->contentsMargins();
    s.rwidth() += m.left() + m.right();
    s.rheight() += m.top() + m.bottom();
    return s;
}

// ---------------------------------------------------------------- tab ADMIN

AdminTab::AdminTab(QWidget *parent)
    : EngineerTab(parent)
{
    m_block = new CommandBlock(Proto::CatCmdAdmin, Proto::CatCmdAdminBack, CmdAdmin::Count,
                               CmdAdmin::defaults(), this);

    QVBoxLayout *lay = nullptr;
    scrollPage(this, &lay);

    lay->addWidget(buildVideoGroup());
    lay->addWidget(buildTxGroup());
    lay->addWidget(buildAkGroup());
    lay->addWidget(buildCalibGroup());
    lay->addWidget(buildCalibResultGroup());
    lay->addStretch(1);

    // Hàng "Gửi lệnh" nằm cố định dưới vùng cuộn để lúc nào cũng bấm được.
    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(8, 4, 8, 8);
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), this);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), this);
    bottom->addWidget(sendBtn);
    bottom->addWidget(sent);
    bottom->addStretch(1);
    qobject_cast<QVBoxLayout *>(layout())->addLayout(bottom);

    m_block->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN"));
    m_block->setSerialLabels(sent, nullptr);

    connect(m_block, &CommandBlock::backSerialChanged, this, &EngineerTab::cornerSerialChanged);
    connect(m_block, &CommandBlock::backReceived, this, [this] {
        // Nút vẽ cánh sóng chỉ mở khi hệ thống MH báo đang ra dữ liệu IQ.
        m_viewIqBtn->setEnabled(m_block->backField(CmdAdmin::ViewIq) > 1);
    });
    connect(m_block, &CommandBlock::sent, this, [this] {
        m_lastSentCalibOnoff = m_block->field(CmdAdmin::CalibOnoff);
        m_flagSendAd = false;
        resetCalibTracking();
    });
    connect(m_block, &CommandBlock::fieldEdited, this, [this](int field, quint32 value) {
        if (field != CmdAdmin::CalibOnoff)
            return;
        updateDeltaEnabled();
        // Đổi chế độ hiệu chuẩn thì phải nạp lại cặp tần số của AD9361 trước
        // khi gửi CMD_ADMIN, nếu không hệ thống hiệu chuẩn ở sai tần số.
        if (value != m_lastSentCalibOnoff) {
            m_flagSendAd = true;
            emit calibPresetRequested(int(value));
        }
    });

    updateDeltaEnabled();
    m_viewIqBtn->setEnabled(false);
}

QGroupBox *AdminTab::buildVideoGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Hiển thị video"), this);
    QVBoxLayout *lay = groupLayout(g);
    m_block->addLocked(g);

    auto *r1 = new FieldRow(QStringLiteral("Cường độ:"), g);
    m_block->bind(r1->add(QString(), new IntEditor(0, 31, g)), CmdAdmin::CuongdoVideo);
    m_block->bind(r1->add(QStringLiteral("Điểm đầu:"), new IntEditor(0, 315, g)),
                  CmdAdmin::DiemdauVideo);

    auto *r2 = new FieldRow(QStringLiteral("Kênh:"), g);
    m_block->bind(r2->add(QString(), new RadioEditor({QStringLiteral("SumF2"), QStringLiteral("SubF2"),
                                                      QStringLiteral("SumF3"), QStringLiteral("SubF3")},
                                                     upto(4), 4, g)),
                  CmdAdmin::KenhVideo);

    auto *r3 = new FieldRow(QStringLiteral("Cửa sổ ngưỡng:"), g);
    m_block->bind(r3->add(QString(), new RadioEditor(kOnOff, k01, 2, g)), CmdAdmin::CuasoNguong);

    lay->addWidget(r1);
    lay->addWidget(r2);
    lay->addWidget(r3);
    LabeledRow::alignTitles({r1, r2, r3});
    return g;
}

QGroupBox *AdminTab::buildTxGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Kênh phát"), this);
    QVBoxLayout *lay = groupLayout(g);
    m_block->addLocked(g);

    const struct { const char *title; int phase; int amp; } kRows[] = {
        {"Tx1 100%:", CmdAdmin::Tx1Phase100, CmdAdmin::Tx1Amp100},
        {"Tx2 100%:", CmdAdmin::Tx2Phase100, CmdAdmin::Tx2Amp100},
        {"Tx1 50%:",  CmdAdmin::Tx1Phase50,  CmdAdmin::Tx1Amp50},
        {"Tx2 50%:",  CmdAdmin::Tx2Phase50,  CmdAdmin::Tx2Amp50},
    };
    QVector<LabeledRow *> rows;
    for (const auto &r : kRows) {
        auto *row = new FieldRow(QString::fromUtf8(r.title), g);
        m_block->bind(row->add(QStringLiteral("Pha:"), new IntEditor(0, 359, g)), r.phase);
        m_block->bind(row->add(QStringLiteral("Biên độ:"), new IntEditor(0, 10000, g)), r.amp);
        lay->addWidget(row);
        rows.append(row);
    }

    auto *docs = new FieldRow(QStringLiteral("Đo CS:"), g);
    m_block->bind(docs->add(QString(), new RadioEditor(kOnOff, k01, 2, g)), CmdAdmin::DoCs);
    lay->addWidget(docs);
    rows.append(docs);

    LabeledRow::alignTitles(rows);
    return g;
}

QGroupBox *AdminTab::buildAkGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Hiệu chỉnh AK"), this);
    QVBoxLayout *lay = groupLayout(g);
    m_block->addLocked(g);

    auto *r1 = new FieldRow(QStringLiteral("Hiệu chỉnh:"), g);
    m_block->bind(r1->add(QString(), new RadioEditor(kOnOff, k01, 2, g)), CmdAdmin::AkTest);

    auto *r2 = new FieldRow(QStringLiteral("Gain:"), g);
    m_block->bind(r2->add(QStringLiteral("KC:"), new IntEditor(0, kIntMax, g)), CmdAdmin::AkGainKc);
    m_block->bind(r2->add(QStringLiteral("KP:"), new IntEditor(0, kIntMax, g)), CmdAdmin::AkGainKp);
    m_block->bind(r2->add(QStringLiteral("KT:"), new IntEditor(0, kIntMax, g)), CmdAdmin::AkGainKt);

    lay->addWidget(r1);
    lay->addWidget(r2);
    LabeledRow::alignTitles({r1, r2});
    return g;
}

QGroupBox *AdminTab::buildCalibGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Hiệu chuẩn"), this);
    QVBoxLayout *lay = groupLayout(g);
    m_block->addLocked(g);

    auto *r1 = new FieldRow(QStringLiteral("Kiểu dữ liệu ra:"), g);
    m_block->bind(r1->add(QString(), new RadioEditor(
                              {QStringLiteral("Plot"), QStringLiteral("Video"),
                               QStringLiteral("Vẽ CS: F2"), QStringLiteral("Vẽ CS: F3"),
                               QStringLiteral("ViewIQ: F2-I"), QStringLiteral("ViewIQ: F2-Q"),
                               QStringLiteral("ViewIQ: F3-I"), QStringLiteral("ViewIQ: F3-Q")},
                              upto(8), 4, g)),
                  CmdAdmin::ViewIq);

    auto *r2 = new FieldRow(QStringLiteral("Hiệu chuẩn:"), g);
    m_block->bind(r2->add(QString(), new RadioEditor(
                              {QStringLiteral("Tắt"), QStringLiteral("F2 Rx"), QStringLiteral("F3 Rx"),
                               QStringLiteral("F4 Rx"), QStringLiteral("F2 Tx"), QStringLiteral("F3 Tx"),
                               QStringLiteral("System-F2"), QStringLiteral("System-F3"),
                               QStringLiteral("System-F4")},
                              upto(9), 5, g)),
                  CmdAdmin::CalibOnoff);

    // Bù K2 nhập theo đơn vị người dùng (1,005) rồi nhân 1000 mới vào gói tin.
    auto *r3 = new FieldRow(QStringLiteral("Bù K2:"), g);
    m_deltaF2 = m_block->bind(r3->add(QStringLiteral("Sys-F2:"),
                                      new DoubleEditor(0.0, 4294967.0, 3, 1000.0, false, 0, g)),
                              CmdAdmin::DeltatxF2);
    m_deltaF3 = m_block->bind(r3->add(QStringLiteral("Sys-F3:"),
                                      new DoubleEditor(0.0, 4294967.0, 3, 1000.0, false, 0, g)),
                              CmdAdmin::DeltatxF3);
    m_deltaF4 = m_block->bind(r3->add(QStringLiteral("Sys-F4:"),
                                      new DoubleEditor(0.0, 4294967.0, 3, 1000.0, false, 0, g)),
                              CmdAdmin::Deltatx);

    m_viewIqBtn = new QPushButton(QStringLiteral("ViewIQ - Vẽ cánh sóng"), g);
    connect(m_viewIqBtn, &QPushButton::clicked, this, &AdminTab::viewIqRequested);
    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_viewIqBtn);
    btnRow->addStretch(1);

    lay->addWidget(r1);
    lay->addWidget(r2);
    lay->addWidget(r3);
    lay->addLayout(btnRow);
    LabeledRow::alignTitles({r1, r2, r3});
    return g;
}

// Ba ô bù K2 chỉ mở đúng ô ứng với chế độ hiệu chuẩn hệ thống đang chọn.
void AdminTab::updateDeltaEnabled()
{
    const quint32 mode = m_block->field(CmdAdmin::CalibOnoff);
    m_deltaF2->setEnabled(mode == 6);
    m_deltaF3->setEnabled(mode == 7);
    m_deltaF4->setEnabled(mode == 8);
}

QGroupBox *AdminTab::buildCalibResultGroup()
{
    auto *g = new QGroupBox(QStringLiteral("Kết quả hiệu chuẩn"), this);
    auto *lay = new QVBoxLayout(g);
    lay->setContentsMargins(10, 4, 10, 6);
    lay->setSpacing(2);

    // Serial của STATUS_CALIB nằm sát bên phải nhóm.
    m_calibSerial = serialLabel(QStringLiteral("Phản hồi: "), g);
    auto *head = new QHBoxLayout;
    head->addStretch(1);
    head->addWidget(m_calibSerial);
    lay->addLayout(head);

    m_calibTable = new QTableWidget(8, 4, g);
    m_calibTable->setHorizontalHeaderLabels({QStringLiteral("Params"), QStringLiteral("Value"),
                                             QStringLiteral("Check"), QStringLiteral("Value")});
    m_calibTable->verticalHeader()->setVisible(false);
    m_calibTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_calibTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_calibTable->setFocusPolicy(Qt::NoFocus);
    m_calibTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    static const char *const kParams[8] = {
        "K3-F2 Rx", "K3-F3 Rx", "K3-F4 Rx", "K3-F2 Tx",
        "K3-F3 Tx", "K56-F2", "K56-F3", "K56-F4",
    };
    static const char *const kChecks[8] = {
        "Read K2", "Read K3", "Read K4", "", "", "", "Tx2 Amp (F4)", "Amp selected",
    };
    // Hai cột chữ có nền khác để nhìn ra ngay đó là cột tiêu đề.
    const QBrush headBrush(QColor(0x26, 0x2c, 0x33));
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 4; ++c) {
            auto *item = new QTableWidgetItem;
            if (c == 0)
                item->setText(QString::fromUtf8(kParams[r]));
            else if (c == 2)
                item->setText(QString::fromUtf8(kChecks[r]));
            else
                item->setText(QString::fromUtf8("—"));
            if (c == 0 || c == 2) {
                item->setBackground(headBrush);
                item->setForeground(QBrush(QColor(0xb9, 0xc3, 0xcd)));
            } else {
                item->setTextAlignment(Qt::AlignCenter);
            }
            m_calibTable->setItem(r, c, item);
        }
    }

    const int rowHeight = qMax(21, fontMetrics().height() + 4);
    m_calibTable->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_calibTable->setFixedHeight(m_calibTable->horizontalHeader()->sizeHint().height()
                                 + rowHeight * 8 + 2 * m_calibTable->frameWidth());
    lay->addWidget(m_calibTable);
    return g;
}

void AdminTab::resetCalibTracking()
{
    m_deltaTxCount = 0;
    m_minAmp = 0;
    m_minDelta = 0.0;
    m_hasMin = false;
}

void AdminTab::applyCalibStatus(const quint32 *fields, quint32 serial)
{
    m_calibSerial->setText(QStringLiteral("Phản hồi: %1").arg(serial));

    // Biên độ đọc theo thang 32768; pha nằm trong gói theo radian nên đổi ra độ.
    const auto pairText = [](qint32 amp, qint32 phase) {
        return QStringLiteral("%1 / %2").arg(round3(amp / 32768.0),
                                             round3(phase / 32768.0 / kPi * 180.0));
    };

    for (int r = 0; r < 8; ++r) {
        const int i = StatusCalib::Feedback0 + r * 2;
        m_calibTable->item(r, 1)->setText(pairText(qint32(fields[i]), qint32(fields[i + 1])));
    }
    for (int r = 0; r < 3; ++r) {
        const int i = StatusCalib::Read0 + r * 2;
        m_calibTable->item(r, 3)->setText(pairText(qint32(fields[i]), qint32(fields[i + 1])));
    }

    // Theo dõi biên độ Tx2 của chế độ hiệu chuẩn System-F4: lần thứ ba tỉ số
    // vượt 1 là điểm biên độ nhỏ nhất cần giữ lại.
    const qint32 amp1 = qint32(fields[StatusCalib::Tx2Amp1]);
    const double raw2 = qint32(fields[StatusCalib::Tx2Amp2]) / 32768.0;
    if (raw2 != 0.0) {
        const double delta = 1.0 / raw2;
        m_calibTable->item(6, 3)->setText(QStringLiteral("%1 / %2")
                                              .arg(QString::number(amp1), round3(delta)));
        if (delta >= 1.0) {
            ++m_deltaTxCount;
            if (m_deltaTxCount == 3) {
                m_minAmp = quint32(amp1);
                m_minDelta = delta;
                m_hasMin = true;
            }
        }
        m_calibTable->item(7, 3)->setText(
            m_hasMin ? QStringLiteral("%1 / %2").arg(QString::number(qint32(m_minAmp)),
                                                     round3(m_minDelta))
                     : QStringLiteral("0 / 0"));
    }
}

QString AdminTab::cornerSerialText() const
{
    return m_block->backSerialText();
}

void AdminTab::setLocked(bool locked)
{
    m_block->setLocked(locked);
    if (!locked)
        updateDeltaEnabled();
}

void AdminTab::clearBack()
{
    m_block->clearBack();
    m_viewIqBtn->setEnabled(false);
    m_calibSerial->setText(QString::fromUtf8("Phản hồi: —"));
    resetCalibTracking();
}

// ------------------------------------------------------------------- tab AD

AdTab::AdTab(QWidget *parent)
    : EngineerTab(parent)
{
    m_block = new CommandBlock(Proto::CatCmdAdminAd, Proto::CatCmdAdminAdBack, CmdAdminAd::Count,
                               CmdAdminAd::defaults(), this);

    QVBoxLayout *lay = nullptr;
    scrollPage(this, &lay);

    QGroupBox *g = makeGroup(QStringLiteral("Điều khiển AD9361"), this);
    QVBoxLayout *glay = groupLayout(g);
    m_block->addLocked(g);

    m_preset = new QComboBox(g);
    for (int i = 0; i < CmdAdminAd::freqPresetCount(); ++i)
        m_preset->addItem(QString::fromUtf8(CmdAdminAd::freqPreset(i).name));
    m_preset->setFocusPolicy(Qt::StrongFocus);
    // Mục dài nhất là "Calib Sys F4"; để Qt tự co theo nội dung thì mũi tên bị
    // cắt mất, nên đặt sàn bề rộng luôn.
    m_preset->setMinimumWidth(150);

    auto *r0 = new FieldRow(QStringLiteral("Chọn tần số:"), g);
    r0->addPlain(QString(), m_preset);
    m_block->bind(r0->add(QString(), new ComboEditor({QStringLiteral("MHz"), QStringLiteral("KHz")}, g)),
                  CmdAdminAd::InputType);

    auto *r1 = new FieldRow(QStringLiteral("Tần số phát:"), g);
    m_block->bind(r1->add(QString(), new IntEditor(0, kIntMax, g)), CmdAdminAd::Ftx);
    auto *r2 = new FieldRow(QStringLiteral("Tần số thu:"), g);
    m_block->bind(r2->add(QString(), new IntEditor(0, kIntMax, g)), CmdAdminAd::Frx);

    auto *r3 = new FieldRow(QStringLiteral("Suy giảm (0..85):"), g);
    m_block->bind(r3->add(QStringLiteral("Tx1:"), new IntEditor(0, 85, g)), CmdAdminAd::GainTx1);
    m_block->bind(r3->add(QStringLiteral("Tx2:"), new IntEditor(0, 85, g)), CmdAdminAd::GainTx2);

    auto *r4 = new FieldRow(QStringLiteral("Khuếch đại (0..70):"), g);
    m_block->bind(r4->add(QStringLiteral("Rx1:"), new IntEditor(0, 70, g)), CmdAdminAd::GainRx1);
    m_block->bind(r4->add(QStringLiteral("Rx2:"), new IntEditor(0, 70, g)), CmdAdminAd::GainRx2);

    for (FieldRow *r : {r0, r1, r2, r3, r4})
        glay->addWidget(r);
    LabeledRow::alignTitles({r0, r1, r2, r3, r4});

    lay->addWidget(g);
    lay->addStretch(1);

    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(8, 4, 8, 8);
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), this);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), this);
    bottom->addWidget(sendBtn);
    bottom->addWidget(sent);
    bottom->addStretch(1);
    qobject_cast<QVBoxLayout *>(layout())->addLayout(bottom);

    m_block->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN_AD"));
    m_block->setSerialLabels(sent, nullptr);
    connect(m_block, &CommandBlock::backSerialChanged, this, &EngineerTab::cornerSerialChanged);

    connect(m_preset, QOverload<int>::of(&QComboBox::activated), this, &AdTab::applyPreset);
}

// Chỉ nạp cặp tần số, không đụng đến những ô khác: sau khi chọn nhanh, kỹ sư
// vẫn sửa được tần số phát/thu bằng tay.
void AdTab::applyPreset(int index)
{
    const CmdAdminAd::FreqPreset &p = CmdAdminAd::freqPreset(index);
    // Bảng tần số ghi theo MHz; đang nhập theo KHz thì nhân lên 1000.
    const quint32 scale = (m_block->field(CmdAdminAd::InputType) == 1) ? 1000u : 1u;
    m_block->setField(CmdAdminAd::Ftx, p.ftx * scale);
    if (p.frx != 0)
        m_block->setField(CmdAdminAd::Frx, p.frx * scale);
}

void AdTab::selectPreset(int index)
{
    if (index < 0 || index >= m_preset->count())
        return;
    m_preset->setCurrentIndex(index);
    applyPreset(index);
}

QString AdTab::cornerSerialText() const
{
    return m_block->backSerialText();
}

void AdTab::setLocked(bool locked)
{
    m_block->setLocked(locked);
    m_preset->setEnabled(!locked);
}

void AdTab::clearBack()
{
    m_block->clearBack();
}

// ------------------------------------------------------------------- tab SW

SwTab::SwTab(QWidget *parent)
    : EngineerTab(parent)
{
    m_block = new CommandBlock(Proto::CatCmdAdminSw, Proto::CatCmdAdminSwBack, CmdAdminSw::Count,
                               CmdAdminSw::defaults(), this);

    QVBoxLayout *lay = nullptr;
    scrollPage(this, &lay);

    QGroupBox *gVideo = makeGroup(QStringLiteral("Hệ số video"), this);
    m_block->addLocked(gVideo);
    auto *v1 = new FieldRow(QStringLiteral("Nhân:"), gVideo);
    m_block->bind(v1->add(QString(), new IntEditor(1, 1000000, gVideo)), CmdAdminSw::VideoMulti);
    m_block->bind(v1->add(QStringLiteral("Chia:"), new IntEditor(1, 1000000, gVideo)),
                  CmdAdminSw::VideoDivi);
    groupLayout(gVideo)->addWidget(v1);

    QGroupBox *gCx = makeGroup(QStringLiteral("Chùm xung"), this);
    m_block->addLocked(gCx);
    auto *c1 = new FieldRow(QStringLiteral("Cx min:"), gCx);
    m_block->bind(c1->add(QString(), new IntEditor(2, 200, gCx)), CmdAdminSw::CxMin);
    m_block->bind(c1->add(QStringLiteral("Cx max:"), new IntEditor(2, 200, gCx)), CmdAdminSw::CxMax);
    auto *c2 = new FieldRow(QStringLiteral("Cx begin:"), gCx);
    m_block->bind(c2->add(QString(), new IntEditor(2, 10, gCx)), CmdAdminSw::CxBegin);
    m_block->bind(c2->add(QStringLiteral("Cx end:"), new IntEditor(2, 100, gCx)), CmdAdminSw::CxEnd);
    groupLayout(gCx)->addWidget(c1);
    groupLayout(gCx)->addWidget(c2);
    LabeledRow::alignTitles({c1, c2});

    QGroupBox *gSrc = makeGroup(QStringLiteral("Dữ liệu gốc"), this);
    m_block->addLocked(gSrc);
    auto *s1 = new FieldRow(QStringLiteral("Plot debug:"), gSrc);
    m_block->bind(s1->add(QString(), new RadioEditor({QStringLiteral("Off"), QStringLiteral("Debug"),
                                                      QStringLiteral("CxOff"), QStringLiteral("M4-Filter")},
                                                     upto(4), 4, gSrc)),
                  CmdAdminSw::EnaPlotDebug);
    auto *s2 = new FieldRow(QStringLiteral("Video source:"), gSrc);
    m_block->bind(s2->add(QString(), new RadioEditor({QStringLiteral("Off"), QStringLiteral("On"),
                                                      QStringLiteral("1/2"), QStringLiteral("1/3"),
                                                      QStringLiteral("1/4")},
                                                     upto(5), 5, gSrc)),
                  CmdAdminSw::EnaVideoSrc);
    auto *s3 = new FieldRow(QStringLiteral("Plot source:"), gSrc);
    m_block->bind(s3->add(QString(), new RadioEditor(kOffOn, k01, 2, gSrc)), CmdAdminSw::EnaPlotSrc);
    for (FieldRow *r : {s1, s2, s3})
        groupLayout(gSrc)->addWidget(r);
    LabeledRow::alignTitles({s1, s2, s3});

    QGroupBox *gOther = makeGroup(QStringLiteral("Khác"), this);
    m_block->addLocked(gOther);
    auto *o1 = new FieldRow(QStringLiteral("Timer (50..500):"), gOther);
    m_block->bind(o1->add(QString(), new IntEditor(50, 500, gOther)), CmdAdminSw::StTimer);
    auto *o2 = new FieldRow(QStringLiteral("Print debug:"), gOther);
    m_block->bind(o2->add(QString(), new ComboEditor(
                              {QStringLiteral("Off"), QStringLiteral("Plot"), QStringLiteral("Status"),
                               QStringLiteral("TxMode"), QStringLiteral("Calib"), QStringLiteral("Params"),
                               QStringLiteral("GpsHead"), QStringLiteral("GpsReg"), QStringLiteral("HD-Time")},
                              gOther)),
                  CmdAdminSw::EnaPrintConsole);
    auto *o3 = new FieldRow(QStringLiteral("GPS on start:"), gOther);
    m_block->bind(o3->add(QString(), new RadioEditor(kOffOn, k01, 2, gOther)), CmdAdminSw::AutoBugps);
    for (FieldRow *r : {o1, o2, o3})
        groupLayout(gOther)->addWidget(r);
    LabeledRow::alignTitles({o1, o2, o3});

    lay->addWidget(gVideo);
    lay->addWidget(gCx);
    lay->addWidget(gSrc);
    lay->addWidget(gOther);
    lay->addStretch(1);

    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(8, 4, 8, 8);
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), this);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), this);
    bottom->addWidget(sendBtn);
    bottom->addWidget(sent);
    bottom->addStretch(1);
    qobject_cast<QVBoxLayout *>(layout())->addLayout(bottom);

    m_block->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN_SW"));
    m_block->setSerialLabels(sent, nullptr);
    connect(m_block, &CommandBlock::backSerialChanged, this, &EngineerTab::cornerSerialChanged);
}

QString SwTab::cornerSerialText() const
{
    return m_block->backSerialText();
}

void SwTab::setLocked(bool locked)
{
    m_block->setLocked(locked);
}

void SwTab::clearBack()
{
    m_block->clearBack();
}

// ---------------------------------------------------------------- tab Other

OtherTab::OtherTab(QWidget *parent)
    : EngineerTab(parent)
{
    m_other = new CommandBlock(Proto::CatCmdAdminOther, Proto::CatCmdAdminOtherBack,
                               CmdAdminOther::Count, CmdAdminOther::defaults(), this);
    m_calibReg = new CommandBlock(Proto::CatCmdAdminCalibReg, Proto::CatCmdAdminCalibRegBack,
                                  CmdAdminCalibReg::Count, CmdAdminCalibReg::defaults(), this);
    m_buphabd = new CommandBlock(Proto::CatCmdAdminBuphabd, Proto::CatCmdAdminBuphabdBack,
                                 CmdAdminBuphabd::Count, CmdAdminBuphabd::defaults(), this);

    QVBoxLayout *lay = nullptr;
    scrollPage(this, &lay);

    lay->addWidget(buildOtherGroup());
    lay->addWidget(buildCalibRegGroup());
    lay->addWidget(buildBuPhaGroup());

    m_rebootBtn = new QPushButton(QStringLiteral("Khởi động lại hệ thống XL MH"), this);
    auto *rebootRow = new QHBoxLayout;
    rebootRow->addWidget(m_rebootBtn);
    rebootRow->addStretch(1);
    lay->addLayout(rebootRow);
    lay->addStretch(1);

    connect(m_rebootBtn, &QPushButton::clicked, this, &OtherTab::rebootRequested);
}

// Nút "Gửi lệnh" và hai nhãn serial dùng chung cho cả ba nhóm của tab này;
// serial phản hồi nằm sát bên phải nhóm chứ không ở góc thanh tab.
QGroupBox *OtherTab::buildOtherGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Khác"), this);
    QVBoxLayout *lay = groupLayout(g);

    auto *r1 = new FieldRow(QStringLiteral("Bù góc:"), g);
    m_other->bind(r1->add(QString(), new RadioEditor({QStringLiteral("Tắt"),
                                                      QStringLiteral("Bù từ GPS"),
                                                      QStringLiteral("Bù bằng tay")},
                                                     upto(3), 3, g)),
                  CmdAdminOther::BuGoc);

    auto *r2 = new FieldRow(QStringLiteral("Giá trị bù (độ):"), g);
    // 4096 đơn vị mã hoá cho 360 độ; tròn đúng một vòng thì quy về 0.
    m_other->bind(r2->add(QString(), new DoubleEditor(0.0, 359.99, 2, 4096.0 / 360.0, false, 4096, g)),
                  CmdAdminOther::GiatriBu);

    auto *r3 = new FieldRow(QStringLiteral("Bù điểm dấu:"), g);
    m_other->bind(r3->add(QStringLiteral("Cự ly (mét):"), new IntEditor(0, 360000, g)),
                  CmdAdminOther::PlotBuCly);
    m_other->bind(r3->add(QStringLiteral("Phương vị (độ):"),
                          new DoubleEditor(-359.99, 359.99, 2, 100.0, true, 0, g)),
                  CmdAdminOther::PlotBuPvi);

    auto *r4 = new FieldRow(QStringLiteral("Hiệu chuẩn cự ly M2 (mét):"), g);
    m_other->bind(r4->add(QString(), new IntEditor(-kIntMax, kIntMax, g)), CmdAdminOther::CalibRM2);

    auto *r5 = new FieldRow(QStringLiteral("Lọc xung:"), g);
    m_other->bind(r5->add(QString(), new IntEditor(0, 65535, g)), CmdAdminOther::Locxung);

    auto *r6 = new FieldRow(QString(), g);
    m_other->bind(r6->add(QString(), new CheckEditor(
                              QStringLiteral("Lưu tham số hệ thống hiện tại trên Card-XL"), g)),
                  CmdAdminOther::LuuThamso);

    for (FieldRow *r : {r1, r2, r3, r4, r5, r6})
        lay->addWidget(r);
    LabeledRow::alignTitles({r1, r2, r3, r4, r5});

    auto *row = new QHBoxLayout;
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), g);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), g);
    auto *back = serialLabel(QStringLiteral("Phản hồi: "), g);
    row->addWidget(sendBtn);
    row->addWidget(sent);
    row->addStretch(1);
    row->addWidget(back);
    lay->addLayout(row);

    m_other->addLocked(g);
    m_other->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN_OTHER"));
    m_other->setSerialLabels(sent, back);
    return g;
}

QGroupBox *OtherTab::buildCalibRegGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Thanh ghi Calib"), this);
    QVBoxLayout *lay = groupLayout(g);

    // Phần thực theo thang 32768, phần ảo nhập bằng độ rồi đổi sang radian.
    const double imScale = 32768.0 / 180.0 * kPi;
    const auto makeRe = [g] { return new DoubleEditor(-65536.0, 65536.0, 3, 32768.0, true, 0, g); };
    const auto makeIm = [g, imScale] {
        return new DoubleEditor(-3600.0, 3600.0, 3, imScale, true, 0, g);
    };

    const struct { const char *title; int re1, im1, re2, im2; } kRows[] = {
        {"WriteReg F2:", CmdAdminCalibReg::F21Re, CmdAdminCalibReg::F21Im,
                         CmdAdminCalibReg::F22Re, CmdAdminCalibReg::F22Im},
        {"WriteReg F3:", CmdAdminCalibReg::F31Re, CmdAdminCalibReg::F31Im,
                         CmdAdminCalibReg::F32Re, CmdAdminCalibReg::F32Im},
        {"WriteReg F4:", CmdAdminCalibReg::F41Re, CmdAdminCalibReg::F41Im,
                         CmdAdminCalibReg::F42Re, CmdAdminCalibReg::F42Im},
    };
    QVector<LabeledRow *> rows;
    for (const auto &r : kRows) {
        auto *row = new FieldRow(QString::fromUtf8(r.title), g);
        // Bốn ô là hai số phức: hai ô của mỗi số kề nhau, hai cụm cách nhau ra.
        m_calibReg->bind(row->add(QString(), makeRe()), r.re1);
        m_calibReg->bind(row->add(QString(), makeIm()), r.im1);
        row->addSpacing(20);
        m_calibReg->bind(row->add(QString(), makeRe()), r.re2);
        m_calibReg->bind(row->add(QString(), makeIm()), r.im2);
        lay->addWidget(row);
        rows.append(row);
    }
    LabeledRow::alignTitles(rows);

    auto *btnRow = new QHBoxLayout;
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), g);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), g);
    auto *back = serialLabel(QStringLiteral("Phản hồi: "), g);
    btnRow->addWidget(sendBtn);
    btnRow->addWidget(sent);
    btnRow->addStretch(1);
    btnRow->addWidget(back);
    lay->addLayout(btnRow);

    m_calibReg->addLocked(g);
    m_calibReg->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN_CALIB_REG"));
    m_calibReg->setSerialLabels(sent, back);
    return g;
}

QGroupBox *OtherTab::buildBuPhaGroup()
{
    QGroupBox *g = makeGroup(QStringLiteral("Bù Pha-Biên độ"), this);
    QVBoxLayout *lay = groupLayout(g);

    auto *r1 = new FieldRow(QStringLiteral("Bù pha (độ):"), g);
    m_buphabd->bind(r1->add(QStringLiteral("F2:"), new IntEditor(-359, 359, g)), CmdAdminBuphabd::BuF2);
    m_buphabd->bind(r1->add(QStringLiteral("F3:"), new IntEditor(-359, 359, g)), CmdAdminBuphabd::BuF3);
    m_buphabd->bind(r1->add(QStringLiteral("F4:"), new IntEditor(-359, 359, g)), CmdAdminBuphabd::BuF4);

    auto *r2 = new FieldRow(QStringLiteral("Bù biên độ:"), g);
    m_buphabd->bind(r2->add(QStringLiteral("F2:"), new IntEditor(1, 2000, g)), CmdAdminBuphabd::BuF2Amp);
    m_buphabd->bind(r2->add(QStringLiteral("F3:"), new IntEditor(1, 2000, g)), CmdAdminBuphabd::BuF3Amp);
    m_buphabd->bind(r2->add(QStringLiteral("F4:"), new IntEditor(1, 2000, g)), CmdAdminBuphabd::BuF4Amp);

    lay->addWidget(r1);
    lay->addWidget(r2);
    LabeledRow::alignTitles({r1, r2});

    auto *btnRow = new QHBoxLayout;
    auto *sendBtn = new QPushButton(QStringLiteral("Gửi lệnh"), g);
    auto *sent = serialLabel(QStringLiteral("Gửi: "), g);
    auto *back = serialLabel(QStringLiteral("Phản hồi: "), g);
    btnRow->addWidget(sendBtn);
    btnRow->addWidget(sent);
    btnRow->addStretch(1);
    btnRow->addWidget(back);
    lay->addLayout(btnRow);

    m_buphabd->addLocked(g);
    m_buphabd->setSendButton(sendBtn, QStringLiteral("CMD_ADMIN_BUPHABD"));
    m_buphabd->setSerialLabels(sent, back);
    return g;
}

void OtherTab::setLocked(bool locked)
{
    m_other->setLocked(locked);
    m_calibReg->setLocked(locked);
    m_buphabd->setLocked(locked);
    m_rebootBtn->setEnabled(!locked);
}

void OtherTab::clearBack()
{
    m_other->clearBack();
    m_calibReg->clearBack();
    m_buphabd->clearBack();
}

// --------------------------------------------------------------- tab Params

ParamsTab::ParamsTab(QWidget *parent)
    : EngineerTab(parent)
{
    // Lề hẹp: bảng 100 dòng nên nhường hết chỗ cho số dòng nhìn thấy được.
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    auto *box = new QGroupBox(QStringLiteral("Danh sách tham số lưu trên hệ thống XL MH"), this);
    auto *boxLay = new QVBoxLayout(box);
    boxLay->setContentsMargins(6, 4, 6, 6);

    m_table = new QTableWidget(StatusParams::kCount, 3, box);
    m_table->setHorizontalHeaderLabels({QStringLiteral("STT"), QStringLiteral("Tên tham số"),
                                        QStringLiteral("Giá trị")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    // Bảng 100 dòng nhìn dễ lạc mắt; nền dòng chẵn lẻ khác nhau một chút. Phải
    // đặt bằng stylesheet chứ không bằng palette: bảng kiểu chung của phần mềm
    // đã gán background cho QTableWidget nên palette không có tác dụng nữa.
    m_table->setStyleSheet(QStringLiteral("QTableWidget { alternate-background-color:#191f25; }"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 110);
    m_table->verticalHeader()->setDefaultSectionSize(qMax(20, fontMetrics().height() + 4));

    for (int i = 0; i < StatusParams::kCount; ++i) {
        auto *stt = new QTableWidgetItem(QString::number(i + 1));
        stt->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 0, stt);
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(StatusParams::name(i))));
        auto *val = new QTableWidgetItem(QStringLiteral("..."));
        val->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(i, 2, val);
    }

    boxLay->addWidget(m_table);
    lay->addWidget(box, 1);
}

void ParamsTab::applyParams(const quint32 *fields, quint32 serial)
{
    m_serial = serial;
    m_hasSerial = true;
    for (int i = 0; i < StatusParams::kCount; ++i) {
        // Tham số như calibR_m2 mặc định -600 nên đọc theo số có dấu.
        const bool used = QLatin1String(StatusParams::name(i)) != QLatin1String("NA");
        m_table->item(i, 2)->setText(used ? QString::number(qint32(fields[i]))
                                          : QStringLiteral("..."));
    }
    emit cornerSerialChanged();
}

QString ParamsTab::cornerSerialText() const
{
    return m_hasSerial ? QStringLiteral("Phản hồi: %1").arg(m_serial)
                       : QString::fromUtf8("Phản hồi: —");
}

void ParamsTab::clearBack()
{
    m_hasSerial = false;
    for (int i = 0; i < StatusParams::kCount; ++i)
        m_table->item(i, 2)->setText(QStringLiteral("..."));
    emit cornerSerialChanged();
}
