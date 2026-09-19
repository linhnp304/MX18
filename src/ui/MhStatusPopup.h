#pragma once

#include "proto/Packets.h"
#include "ui/SlidePopup.h"

#include <QVector>

class QLabel;

// Cửa sổ "Trạng thái MH": đổ dữ liệu từ gói STATUS_MH (1 gói/giây).
//
// Giá trị vượt ngưỡng trong ./settings/statuserror.json thì đổi chữ sang đỏ.
class MhStatusPopup : public SlidePopup
{
    Q_OBJECT
public:
    explicit MhStatusPopup(QWidget *parent);

    // serial lấy từ khung gói tin chứ không phải một trường data_fields[].
    void setStatus(const quint32 *fields, quint32 serial);

    // Trạng thái phản hồi của noiphat (CMD_USER, category 0x90010): chỉ khi đang
    // nối phát mới báo lỗi công suất thấp.
    void setTransmitOn(bool on) { m_transmitOn = on; }

    void clearStatus();

    // Có ít nhất một giá trị vượt ngưỡng ở gói vừa nhận: biểu tượng trên thanh
    // trạng thái chuyển sang đỏ.
    bool hasError() const { return m_anyError; }

private:
    // Một ô giá trị: nhãn bên trái, một hoặc hai cột giá trị bên phải.
    QLabel *addRow(class QGridLayout *grid, int row, const QString &caption, int column = 0,
                   int columnCount = 1);
    class QGridLayout *addGroup(const QString &title, const QStringList &valueHeaders);
    void setValue(QLabel *label, const QString &text, bool error);

    // K2
    QLabel *m_k2_50v = nullptr;
    QLabel *m_k2_5v = nullptr;
    QLabel *m_k2_m5v = nullptr;
    QLabel *m_k2_t = nullptr;
    QLabel *m_k2_h = nullptr;

    // K5.1 - K5.2 (cột 0: các trường k6_tx2_*, cột 1: các trường k5_tx1_*)
    QLabel *m_tx_cs[2] = {nullptr, nullptr};
    QLabel *m_tx_hssd[2] = {nullptr, nullptr};
    QLabel *m_tx_t[2] = {nullptr, nullptr};
    QLabel *m_tx_h[2] = {nullptr, nullptr};
    QLabel *m_tx_stc[2] = {nullptr, nullptr};
    QLabel *m_tx_ctr[2] = {nullptr, nullptr};

    // K3
    QLabel *m_k3_key = nullptr;
    QLabel *m_k3_keytime = nullptr;
    QLabel *m_k3_t = nullptr;
    QLabel *m_k3_h = nullptr;
    QLabel *m_k3_beta = nullptr;

    // GPS
    QLabel *m_gps_heading = nullptr;
    QLabel *m_gps_lat = nullptr;
    QLabel *m_gps_lng = nullptr;
    QLabel *m_gps_state = nullptr;

    // Khác
    QLabel *m_giatribu = nullptr;
    QLabel *m_serial = nullptr;

    QVector<QLabel *> m_allValues;
    int m_gpsCountErr = 0;
    bool m_transmitOn = false;
    bool m_anyError = false;
};
