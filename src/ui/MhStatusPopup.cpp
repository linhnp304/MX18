#include "ui/MhStatusPopup.h"

#include "core/Settings.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <cmath>
#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

const char *const kNoData = "—";

QString fixed(double v, int decimals)
{
    return QString::number(v, 'f', decimals);
}

} // namespace

MhStatusPopup::MhStatusPopup(QWidget *parent)
    : SlidePopup(QStringLiteral("Trạng thái MH"), SlidePopup::FromLeft, parent)
{
    auto *lay = new QVBoxLayout(body());
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    QGridLayout *k2 = addGroup(QStringLiteral("K2"), {});
    m_k2_50v = addRow(k2, 0, QStringLiteral("Nguồn 50V"));
    m_k2_5v  = addRow(k2, 1, QStringLiteral("Nguồn 5V"));
    m_k2_m5v = addRow(k2, 2, QStringLiteral("Nguồn -5V"));
    m_k2_t   = addRow(k2, 3, QStringLiteral("Nhiệt độ (ºC)"));
    m_k2_h   = addRow(k2, 4, QStringLiteral("Độ ẩm (%)"));

    QGridLayout *tx = addGroup(QStringLiteral("K5.1 - K5.2"),
                               {QStringLiteral("K5.1"), QStringLiteral("K5.2")});
    const struct { const char *caption; QLabel **slot; } kTxRows[] = {
        {"Công suất",          m_tx_cs},
        {"Công suất phản hồi", m_tx_hssd},
        {"Nhiệt độ (ºC)",      m_tx_t},
        {"Độ ẩm (%)",          m_tx_h},
        {"STC",                m_tx_stc},
        {"Chế độ",             m_tx_ctr},
    };
    for (int r = 0; r < int(sizeof(kTxRows) / sizeof(kTxRows[0])); ++r) {
        for (int c = 0; c < 2; ++c) {
            kTxRows[r].slot[c] = addRow(tx, r + 1, c == 0 ? QString::fromUtf8(kTxRows[r].caption)
                                                          : QString(), c, 2);
        }
    }

    QGridLayout *k3 = addGroup(QStringLiteral("K3"), {});
    m_k3_key     = addRow(k3, 0, QStringLiteral("Khóa M2"));
    m_k3_keytime = addRow(k3, 1, QStringLiteral("Thời gian M2"));
    m_k3_t       = addRow(k3, 2, QStringLiteral("Nhiệt độ (ºC)"));
    m_k3_h       = addRow(k3, 3, QStringLiteral("Độ ẩm (%)"));
    m_k3_beta    = addRow(k3, 4, QStringLiteral("Phương vị Enc"));

    QGridLayout *gps = addGroup(QStringLiteral("GPS"), {});
    m_gps_heading = addRow(gps, 0, QStringLiteral("Hướng (độ)"));
    m_gps_lat     = addRow(gps, 1, QStringLiteral("Vỹ độ"));
    m_gps_lng     = addRow(gps, 2, QStringLiteral("Kinh độ"));
    m_gps_state   = addRow(gps, 3, QStringLiteral("Trạng thái"));

    QGridLayout *other = addGroup(QStringLiteral("Khác"), {});
    m_giatribu = addRow(other, 0, QStringLiteral("Bù phương vị MH (độ)"));
    m_serial   = addRow(other, 1, QStringLiteral("Serial"));

    clearStatus();
}

QGridLayout *MhStatusPopup::addGroup(const QString &title, const QStringList &valueHeaders)
{
    auto *box = new QGroupBox(title, body());
    auto *grid = new QGridLayout(box);
    grid->setContentsMargins(8, 4, 8, 6);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(0, 1);

    for (int i = 0; i < valueHeaders.size(); ++i) {
        auto *h = new QLabel(valueHeaders.at(i), box);
        h->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->setStyleSheet(QStringLiteral("color:#7fc4ff;font-weight:bold;"));
        grid->addWidget(h, 0, i + 1);
    }

    qobject_cast<QVBoxLayout *>(body()->layout())->addWidget(box);
    return grid;
}

QLabel *MhStatusPopup::addRow(QGridLayout *grid, int row, const QString &caption, int column,
                              int columnCount)
{
    if (!caption.isEmpty()) {
        auto *lbl = new QLabel(caption, grid->parentWidget());
        lbl->setStyleSheet(QStringLiteral("color:#b9c3cd;"));
        grid->addWidget(lbl, row, 0);
    }

    auto *value = new QLabel(QString::fromUtf8(kNoData), grid->parentWidget());
    // Mọi giá trị căn phải và rộng bằng nhau thì các cột số thẳng hàng dọc.
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value->setMinimumWidth(columnCount > 1 ? 74 : 120);
    value->setStyleSheet(QStringLiteral("color:#e2e8ef;font-weight:bold;"));
    grid->addWidget(value, row, column + 1);
    m_allValues.append(value);
    return value;
}

void MhStatusPopup::setValue(QLabel *label, const QString &text, bool error)
{
    if (error)
        m_anyError = true;
    label->setText(text);
    label->setStyleSheet(error ? QStringLiteral("color:#ff4d4d;font-weight:bold;")
                               : QStringLiteral("color:#e2e8ef;font-weight:bold;"));
}

void MhStatusPopup::clearStatus()
{
    for (QLabel *l : std::as_const(m_allValues))
        setValue(l, QString::fromUtf8(kNoData), false);
    m_gpsCountErr = 0;
    m_anyError = false;
}

void MhStatusPopup::setStatus(const quint32 *f, quint32 serial)
{
    const StatusLimits &lim = Settings::instance().statusLimits();
    m_anyError = false;

    // --- K2: ba mức nguồn ghi ở đơn vị 0,1 V
    const double v50 = f[StatusMh::K2Nguon50V] / 10.0;
    setValue(m_k2_50v, fixed(v50, 1), v50 < lim.min50V || v50 > lim.max50V);

    const double v5 = f[StatusMh::K2Nguon5V] / 10.0;
    setValue(m_k2_5v, fixed(v5, 1), v5 < lim.min5V || v5 > lim.max5V);

    const double vm5 = f[StatusMh::K2NguonM5V] / -10.0;
    setValue(m_k2_m5v, fixed(vm5, 1),
             std::fabs(vm5) < lim.min5V || std::fabs(vm5) > lim.max5V);

    setValue(m_k2_t, QString::number(f[StatusMh::K2Nhietdo]),
             int(f[StatusMh::K2Nhietdo]) > lim.maxT);
    setValue(m_k2_h, QString::number(f[StatusMh::K2Doam]),
             int(f[StatusMh::K2Doam]) > lim.maxH);

    // --- K5.1 / K5.2: cột 0 là k6_tx2_*, cột 1 là k5_tx1_*
    const StatusMh::Field kCs[2]   = {StatusMh::K6Tx2Cs, StatusMh::K5Tx1Cs};
    const StatusMh::Field kHssd[2] = {StatusMh::K6Tx2Hssd, StatusMh::K5Tx1Hssd};
    const StatusMh::Field kT[2]    = {StatusMh::K6Tx2Nhietdo, StatusMh::K5Tx1Nhietdo};
    const StatusMh::Field kH[2]    = {StatusMh::K6Tx2Doam, StatusMh::K5Tx1Doam};
    const StatusMh::Field kStc[2]  = {StatusMh::K6StcBack, StatusMh::K5StcBack};
    const StatusMh::Field kCtr[2]  = {StatusMh::K6CtrBack, StatusMh::K5CtrBack};

    for (int c = 0; c < 2; ++c) {
        // Công suất thấp chỉ là lỗi khi đang nối phát; lúc tắt phát thì đương
        // nhiên bằng 0.
        setValue(m_tx_cs[c], QString::number(f[kCs[c]]),
                 m_transmitOn && int(f[kCs[c]]) < lim.minCs);
        setValue(m_tx_hssd[c], QString::number(f[kHssd[c]]), false);
        setValue(m_tx_t[c], QString::number(f[kT[c]]), int(f[kT[c]]) > lim.maxT);
        setValue(m_tx_h[c], QString::number(f[kH[c]]), int(f[kH[c]]) > lim.maxH);
        setValue(m_tx_stc[c], QString::number(f[kStc[c]]), false);
        setValue(m_tx_ctr[c], QString::number(f[kCtr[c]]), false);
    }

    // --- K3
    const quint32 key = f[StatusMh::K3Key];
    QString keyText;
    switch (key) {
    case 1: keyText = QStringLiteral("HT"); break;
    case 2: keyText = QStringLiteral("KT"); break;
    case 3: keyText = QStringLiteral("HT+KT"); break;
    default: keyText = QStringLiteral("Đã xóa"); break;
    }
    setValue(m_k3_key, keyText, key == 0);
    setValue(m_k3_keytime, StatusMh::keyTimeText(f[StatusMh::KeyTime]), false);
    setValue(m_k3_t, QString::number(f[StatusMh::K3Nhietdo]),
             int(f[StatusMh::K3Nhietdo]) > lim.maxT);
    setValue(m_k3_h, QString::number(f[StatusMh::K3Doam]),
             int(f[StatusMh::K3Doam]) > lim.maxH);
    setValue(m_k3_beta, QString::number(f[StatusMh::K3BetaBack]), false);

    // --- GPS
    const quint32 gps = f[StatusMh::GpsStatus];
    setValue(m_gps_heading, fixed(StatusMh::gpsHeading(gps), 3), false);
    setValue(m_gps_lat, fixed(f[StatusMh::GpsLat] / 10000.0, 4), false);
    setValue(m_gps_lng, fixed(f[StatusMh::GpsLng] / 10000.0, 4), false);

    if (StatusMh::gpsFixGood(gps)) {
        m_gpsCountErr = 0;
        setValue(m_gps_state, QStringLiteral("Tốt"), false);
    } else {
        ++m_gpsCountErr;
        // Mười gói liên tiếp mà tín hiệu chưa ổn định mới coi là hỏng GPS.
        const bool bad = m_gpsCountErr > 10;
        setValue(m_gps_state, bad ? QStringLiteral("Lỗi") : QStringLiteral("Chờ"), bad);
    }

    // --- Khác
    const double bu = std::round(f[StatusMh::Giatribu] * 360.0 / 4096.0 * 1000.0) / 1000.0;
    setValue(m_giatribu, fixed(bu, 3), qFuzzyIsNull(bu));
    setValue(m_serial, QString::number(serial), false);
}
