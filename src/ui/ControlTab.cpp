#include "ui/ControlTab.h"

#include "ui/ControlWidgets.h"

#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

const QStringList kOnOff = {QStringLiteral("Tắt"), QStringLiteral("Bật")};
const QVector<quint32> k01 = {0, 1};

// Dãy lựa chọn đánh số liên tiếp (mã trả lời, vận tốc quay…).
QStringList numberLabels(int lo, int hi)
{
    QStringList out;
    for (int i = lo; i <= hi; ++i)
        out << QString::number(i);
    return out;
}

QVector<quint32> numberValues(int lo, int hi)
{
    QVector<quint32> out;
    for (int i = lo; i <= hi; ++i)
        out << quint32(i);
    return out;
}

QVBoxLayout *groupLayout(QGroupBox *g)
{
    return qobject_cast<QVBoxLayout *>(g->layout());
}

} // namespace

ControlTab::ControlTab(QWidget *parent)
    : QWidget(parent)
{
    memcpy(m_at, CmdAt::defaults(), sizeof(m_at));
    memcpy(m_user, CmdUser::defaults(), sizeof(m_user));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- panel cố định trên cùng
    m_lockBtn = new QPushButton(this);
    m_lockBtn->setMinimumHeight(28);
    auto *top = new QVBoxLayout;
    top->setContentsMargins(8, 8, 8, 4);
    top->addWidget(m_lockBtn);
    outer->addLayout(top);

    // --- vùng cuộn chứa các nhóm lệnh
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *page = new QWidget(scroll);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 4, 8, 8);
    lay->setSpacing(6);

    buildAntenna(page);
    buildMh(page);
    buildCodes(page);
    buildTransmit(page);
    buildDetect(page);
    buildService(page);
    for (QGroupBox *g : std::as_const(m_groups))
        lay->addWidget(g);
    lay->addStretch(1);

    scroll->setWidget(page);
    outer->addWidget(scroll, 1);

    // --- panel cố định dưới cùng
    m_engineerBtn = new QPushButton(QStringLiteral("Điều khiển và thiết lập mức kỹ sư"), this);
    m_engineerBtn->setMinimumHeight(28);
    auto *bottom = new QVBoxLayout;
    bottom->setContentsMargins(8, 4, 8, 8);
    bottom->addWidget(m_engineerBtn);
    outer->addLayout(bottom);

    connect(m_lockBtn, &QPushButton::clicked, this, [this] {
        if (!m_unlocked && !m_connected) {
            emit unlockDenied();
            return;
        }
        setUnlocked(!m_unlocked);
    });
    connect(m_engineerBtn, &QPushButton::clicked, this, &ControlTab::engineerRequested);

    updateModeOptions();
    updateGiaquayEnabled();
    setUnlocked(false);
}

QGroupBox *ControlTab::addGroup(const QString &title, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    auto *lay = new QVBoxLayout(g);
    lay->setContentsMargins(8, 4, 8, 6);
    lay->setSpacing(2);
    m_groups.append(g);
    return g;
}

void ControlTab::bind(RadioRow *row, quint32 *slot, bool isAntenna)
{
    row->setValue(*slot);
    connect(row, &RadioRow::valueChanged, this, [this, slot, isAntenna](quint32 v) {
        *slot = v;
        if (isAntenna)
            emit cmdAtChanged();
        else
            emit cmdUserChanged();
    });
}

void ControlTab::bind(SpinRow *row, quint32 *slot, bool isAntenna)
{
    row->setValue(*slot);
    connect(row, &SpinRow::valueChanged, this, [this, slot, isAntenna](quint32 v) {
        *slot = v;
        if (isAntenna)
            emit cmdAtChanged();
        else
            emit cmdUserChanged();
    });
}

// ------------------------------------------------------- Điều khiển ăng ten

void ControlTab::buildAntenna(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Điều khiển ăng ten"), parent);
    QVBoxLayout *lay = groupLayout(g);

    auto *onoff = new RadioRow(QStringLiteral("Quay ăng ten"),
                               {QStringLiteral("Dừng"), QStringLiteral("Quay")}, k01, 0, g);
    auto *speed = new RadioRow(QStringLiteral("VT quay (v/p)"),
                               numberLabels(1, 6), numberValues(1, 6), 6, g);
    auto *sync = new RadioRow(QStringLiteral("Chế độ quay"),
                              {QStringLiteral("Độc lập"), QStringLiteral("Đồng bộ")}, k01, 0, g);

    bind(onoff, &m_at[CmdAt::AntenOnoff], true);
    bind(speed, &m_at[CmdAt::AntenSpeed], true);
    bind(sync, &m_at[CmdAt::AntenSync], true);

    lay->addWidget(onoff);
    lay->addWidget(speed);
    lay->addWidget(sync);
    LabeledRow::alignTitles({onoff, speed, sync});
}

// ---------------------------------------------------------- Điều khiển MH

void ControlTab::buildMh(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Điều khiển MH"), parent);
    QVBoxLayout *lay = groupLayout(g);

    auto *nguonCs = new RadioRow(QStringLiteral("Nguồn 50V"), kOnOff, {0, 100}, 0, g);
    m_nguonPvi = new RadioRow(QStringLiteral("Đường quét"),
                              {QStringLiteral("Giả quay"), QStringLiteral("Encoder")}, k01, 0, g);
    m_vantocGiaquay = new RadioRow(QStringLiteral("VT giả quay"),
                                   {QStringLiteral("6 v/p"), QStringLiteral("12 v/p")}, k01, 0, g);
    auto *cdLamviec = new RadioRow(QStringLiteral("CĐ làm việc"),
                                   {QStringLiteral("Tạo giả"), QStringLiteral("Làm việc"),
                                    QStringLiteral("TLKT")}, {0, 1, 2}, 0, g);
    auto *giaBd = new RadioRow(QStringLiteral("Giả báo động"), kOnOff, k01, 0, g);
    auto *giaBn = new RadioRow(QStringLiteral("Giả báo nạn"), kOnOff, k01, 0, g);

    bind(nguonCs, &m_user[CmdUser::NguonCs], false);
    bind(m_nguonPvi, &m_user[CmdUser::NguonPvi], false);
    bind(m_vantocGiaquay, &m_user[CmdUser::VantocGiaquay], false);
    bind(cdLamviec, &m_user[CmdUser::CdLamviec], false);
    bind(giaBd, &m_user[CmdUser::GiaBd], false);
    bind(giaBn, &m_user[CmdUser::GiaBn], false);

    // Vận tốc giả quay chỉ có nghĩa khi đường quét lấy từ bộ giả quay.
    connect(m_nguonPvi, &RadioRow::valueChanged, this, [this] { updateGiaquayEnabled(); });

    lay->addWidget(nguonCs);
    lay->addWidget(m_nguonPvi);
    lay->addWidget(m_vantocGiaquay);
    lay->addWidget(cdLamviec);
    lay->addWidget(giaBd);
    lay->addWidget(giaBn);
    LabeledRow::alignTitles({nguonCs, m_nguonPvi, m_vantocGiaquay, cdLamviec, giaBd, giaBn});
}

void ControlTab::updateGiaquayEnabled()
{
    if (m_vantocGiaquay)
        m_vantocGiaquay->setEnabled(m_user[CmdUser::NguonPvi] == 0);
}

// ------------------------------------------------------------ Mã hỏi đáp

void ControlTab::buildCodes(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Mã hỏi đáp"), parent);
    QVBoxLayout *lay = groupLayout(g);

    m_icode1 = new RadioRow(QStringLiteral("Mã hỏi M1"),
                            {QStringLiteral("Máy bay"), QStringLiteral("Tàu biển")}, k01, 0, g);
    m_mode = new RadioRow(QStringLiteral("Chế độ hỏi"),
                          numberLabels(1, 5), numberValues(1, 5), 5, g);
    auto *rcode1 = new RadioRow(QStringLiteral("Mã trả lời M1"),
                                numberLabels(1, 12), numberValues(1, 12), 6, g);
    auto *icode3 = new RadioRow(QStringLiteral("Mã hỏi M3"),
                                numberLabels(1, 6), numberValues(1, 6), 6, g);
    auto *rcode3 = new RadioRow(QStringLiteral("Mã trả lời M3"),
                                numberLabels(1, 7), numberValues(1, 7), 7, g);
    m_keyM2 = new RadioRow(QStringLiteral("Khóa M2"),
                           {QStringLiteral("TĐ"), QStringLiteral("HT"), QStringLiteral("KT"),
                            QStringLiteral("HT+KT")}, {0, 1, 2, 3}, 4, g);

    // Lựa chọn thứ năm của "Chế độ hỏi" đổi theo loại mục tiêu đang hỏi. Phải
    // nối trước bind() để mode được chỉnh xong rồi gói lệnh mới đóng và gửi đi,
    // nếu không gói đầu tiên sau khi đổi loại mục tiêu mang mode cũ.
    connect(m_icode1, &RadioRow::valueChanged, this, [this] { updateModeOptions(); });

    bind(m_icode1, &m_user[CmdUser::Icode1], false);
    bind(m_mode, &m_user[CmdUser::Mode], false);
    bind(rcode1, &m_user[CmdUser::Rcode1], false);
    bind(icode3, &m_user[CmdUser::Icode3], false);
    bind(rcode3, &m_user[CmdUser::Rcode3], false);
    bind(m_keyM2, &m_user[CmdUser::KeyM2], false);

    m_clearKeyBtn = new QPushButton(QStringLiteral("Xóa khóa"), g);
    connect(m_clearKeyBtn, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(this, QStringLiteral("Xóa khóa M2"),
                                                  QStringLiteral("Xóa khóa M2?"),
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        // Xoá khoá là lệnh tức thời: gửi một gói với key_m2=4 rồi trả trường về
        // đúng lựa chọn đang hiện trên giao diện.
        const quint32 keep = m_keyM2->value();
        m_user[CmdUser::KeyM2] = 4;
        emit cmdUserChanged();
        m_user[CmdUser::KeyM2] = keep;
    });

    lay->addWidget(m_icode1);
    lay->addWidget(m_mode);
    lay->addWidget(rcode1);
    lay->addWidget(icode3);
    lay->addWidget(rcode3);
    lay->addWidget(m_keyM2);
    lay->addWidget(m_clearKeyBtn);
    LabeledRow::alignTitles({m_icode1, m_mode, rcode1, icode3, rcode3, m_keyM2});
}

void ControlTab::updateModeOptions()
{
    if (!m_mode || !m_icode1)
        return;
    // Lấy từ hàng điều khiển chứ không từ m_user: hàm này chạy trước lúc giá
    // trị mới được ghi vào gói lệnh.
    const bool tauBien = (m_icode1->value() == 1);
    const quint32 v = tauBien ? 5u : 6u;
    m_mode->setOption(4, QString::number(v), v);

    // Đang chọn ô thứ năm thì giá trị gửi đi phải đổi theo.
    if (m_user[CmdUser::Mode] == 5u || m_user[CmdUser::Mode] == 6u)
        m_user[CmdUser::Mode] = v;
    m_mode->setValue(m_user[CmdUser::Mode]);
}

// ------------------------------------------------------- Điều khiển phát

void ControlTab::buildTransmit(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Điều khiển phát"), parent);
    QVBoxLayout *lay = groupLayout(g);

    auto *noiphat = new RadioRow(QStringLiteral("Nối/Tắt phát"),
                                 {QStringLiteral("Tắt phát"), QStringLiteral("Nối phát")}, k01, 0, g);
    auto *kenhphu = new RadioRow(QStringLiteral("Kênh phụ"), kOnOff, k01, 0, g);
    auto *csPhat = new RadioRow(QStringLiteral("Công suất phát"),
                                {QStringLiteral("50%"), QStringLiteral("100%")}, k01, 0, g);
    auto *cdPhat = new RadioRow(QStringLiteral("Chế độ phát"),
                                {QStringLiteral("Liên tục"), QStringLiteral("Rẻ quạt"),
                                 QStringLiteral("Ngắt 1v"), QStringLiteral("Ngắt 2v")},
                                {0, 1, 2, 3}, 4, g);

    bind(noiphat, &m_user[CmdUser::Noiphat], false);
    bind(kenhphu, &m_user[CmdUser::Kenhphu], false);
    bind(csPhat, &m_user[CmdUser::CsPhat], false);
    bind(cdPhat, &m_user[CmdUser::CdPhat], false);

    m_fan1 = new DualSpinRow(QStringLiteral("Rẻ quạt 1 (độ)"), QStringLiteral("PV đầu"),
                             QStringLiteral("PV cuối"), 0, 359, g);
    m_fan2 = new DualSpinRow(QStringLiteral("Rẻ quạt 2 (độ)"), QStringLiteral("PV đầu"),
                             QStringLiteral("PV cuối"), 0, 359, g);
    m_fan1->setValues(m_user[CmdUser::Azm1], m_user[CmdUser::Azm2]);
    m_fan2->setValues(m_user[CmdUser::Azm3], m_user[CmdUser::Azm4]);
    connect(m_fan1, &DualSpinRow::valueChanged, this, [this] {
        m_user[CmdUser::Azm1] = m_fan1->valueA();
        m_user[CmdUser::Azm2] = m_fan1->valueB();
        emit cmdUserChanged();
    });
    connect(m_fan2, &DualSpinRow::valueChanged, this, [this] {
        m_user[CmdUser::Azm3] = m_fan2->valueA();
        m_user[CmdUser::Azm4] = m_fan2->valueB();
        emit cmdUserChanged();
    });

    lay->addWidget(noiphat);
    lay->addWidget(kenhphu);
    lay->addWidget(csPhat);
    lay->addWidget(cdPhat);
    lay->addWidget(m_fan1);
    lay->addWidget(m_fan2);
    LabeledRow::alignTitles({noiphat, kenhphu, csPhat, cdPhat});
}

// ----------------------------------------------------- Hệ thống phát hiện

void ControlTab::buildDetect(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Hệ thống phát hiện"), parent);
    QVBoxLayout *lay = groupLayout(g);

    auto *stc = new RadioRow(QStringLiteral("STC"), numberLabels(0, 3), numberValues(0, 3), 4, g);
    auto *cnKdb = new RadioRow(QStringLiteral("Chống nhiễu KĐB"), kOnOff, k01, 0, g);
    auto *cnAk = new RadioRow(QStringLiteral("Chống nhiễu AK"), kOnOff, k01, 0, g);
    auto *mono = new RadioRow(QStringLiteral("XL đơn xung"), kOnOff, k01, 0, g);
    auto *mono1 = new SpinRow(QStringLiteral("Hệ số XL đơn xung 1"), 0, 65535, g);
    auto *mono2 = new SpinRow(QStringLiteral("Hệ số XL đơn xung 2"), 0, 65535, g);
    auto *hsAk = new SpinRow(QStringLiteral("Hệ số AK"), 0, 65535, g);
    auto *th1 = new SpinRow(QStringLiteral("Ngưỡng phát hiện 1"), 0, 65535, g);
    auto *th2 = new SpinRow(QStringLiteral("Ngưỡng phát hiện 2"), 0, 65535, g);

    bind(stc, &m_user[CmdUser::HsStc], false);
    bind(cnKdb, &m_user[CmdUser::CnKdb], false);
    bind(cnAk, &m_user[CmdUser::CnAk], false);
    bind(mono, &m_user[CmdUser::Monopulse], false);
    bind(mono1, &m_user[CmdUser::NguongMonopulse], false);
    bind(mono2, &m_user[CmdUser::NguongMonopulse2], false);
    bind(hsAk, &m_user[CmdUser::HsAk], false);
    bind(th1, &m_user[CmdUser::NguongXungdon], false);
    bind(th2, &m_user[CmdUser::NguongXungdon2], false);

    const QVector<LabeledRow *> rows = {stc, cnKdb, cnAk, mono, mono1, mono2, hsAk, th1, th2};
    for (LabeledRow *row : rows)
        lay->addWidget(row);
    LabeledRow::alignTitles(rows);
}

// -------------------------------------------------- Điều khiển dịch vụ VQ

void ControlTab::buildService(QWidget *parent)
{
    QGroupBox *g = addGroup(QStringLiteral("Điều khiển dịch vụ VQ"), parent);
    QVBoxLayout *lay = groupLayout(g);

    // Ba lựa chọn này chỉ đổi cách phần mềm xử lý dữ liệu, không nằm trong gói
    // lệnh nào; phần việc thật nối vào ở giai đoạn sau.
    auto *fromRd = new RadioRow(QStringLiteral("Nhận từ RD"), kOnOff, k01, 0, g);
    auto *toSch = new RadioRow(QStringLiteral("Gửi đến SCH"), kOnOff, k01, 0, g);
    auto *fromPlot = new RadioRow(QStringLiteral("Tạo quỹ đạo MH"), kOnOff, k01, 0, g);
    auto *sendPlot = new RadioRow(QStringLiteral("Gửi điểm dấu MH"), kOnOff, k01, 0, g);
    fromRd->setValue(1);
    toSch->setValue(1);
    fromPlot->setValue(0);
    sendPlot->setValue(1);

    lay->addWidget(fromRd);
    lay->addWidget(toSch);
    lay->addWidget(fromPlot);
    lay->addWidget(sendPlot);
    LabeledRow::alignTitles({fromRd, toSch, fromPlot, sendPlot});
}

// ------------------------------------------------------------ khoá / mở khoá

void ControlTab::setUnlocked(bool unlocked)
{
    m_unlocked = unlocked;
    m_lockBtn->setText(unlocked ? QStringLiteral("Khóa điều khiển")
                                : QStringLiteral("Mở khóa điều khiển"));
    // Đổi màu chữ để nhìn lướt cũng biết đang khoá hay đang mở.
    m_lockBtn->setStyleSheet(unlocked ? QStringLiteral("color:#ff8a5c;font-weight:bold;")
                                      : QStringLiteral("color:#7ee08a;font-weight:bold;"));
    for (QGroupBox *g : std::as_const(m_groups))
        g->setEnabled(unlocked);
    if (unlocked)
        updateGiaquayEnabled();
    emit lockChanged(unlocked);
}

void ControlTab::setSystemConnected(bool connected)
{
    m_connected = connected;
    // Dừng kết nối thì khoá lại ngay: lệnh gửi đi lúc này không tới được MH.
    if (!connected && m_unlocked)
        setUnlocked(false);
}
