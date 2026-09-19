#pragma once

#include "proto/Packets.h"

#include <QVector>
#include <QWidget>

class DualSpinRow;
class RadioRow;
class SpinRow;
class QGroupBox;
class QPushButton;

// Tab "Điều khiển" của panel 2.
//
// Giữ nguyên một bản sao đầy đủ của hai gói lệnh CMD_AT và CMD_USER. Mỗi lần
// trắc thủ đổi một điều khiển thì cả gói được đóng lại và gửi đi một lần (anh
// Linh chốt: không lặp lại định kỳ), nên trạng thái hiển thị luôn là trạng thái
// vừa gửi.
class ControlTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlTab(QWidget *parent = nullptr);

    bool isUnlocked() const { return m_unlocked; }
    void setUnlocked(bool unlocked);

    // Mở khoá điều khiển chỉ có nghĩa khi đang kết nối hệ thống.
    void setSystemConnected(bool connected);

    const quint32 *cmdAtFields() const { return m_at; }
    const quint32 *cmdUserFields() const { return m_user; }

signals:
    void engineerRequested();
    void lockChanged(bool unlocked);
    void unlockDenied();                 // bấm mở khoá khi chưa kết nối
    void cmdAtChanged();
    void cmdUserChanged();

private:
    QGroupBox *addGroup(const QString &title, QWidget *parent);
    void buildAntenna(QWidget *parent);
    void buildMh(QWidget *parent);
    void buildCodes(QWidget *parent);
    void buildTransmit(QWidget *parent);
    void buildDetect(QWidget *parent);
    void buildService(QWidget *parent);

    // Nối một hàng điều khiển vào đúng trường của gói lệnh.
    void bind(RadioRow *row, quint32 *slot, bool isAntenna);
    void bind(SpinRow *row, quint32 *slot, bool isAntenna);

    void updateModeOptions();
    void updateGiaquayEnabled();

    quint32 m_at[CmdAt::Count];
    quint32 m_user[CmdUser::Count];

    QPushButton *m_lockBtn = nullptr;
    QPushButton *m_engineerBtn = nullptr;
    QPushButton *m_clearKeyBtn = nullptr;
    QVector<QGroupBox *> m_groups;

    RadioRow *m_nguonPvi = nullptr;
    RadioRow *m_vantocGiaquay = nullptr;
    RadioRow *m_icode1 = nullptr;
    RadioRow *m_mode = nullptr;
    RadioRow *m_keyM2 = nullptr;
    DualSpinRow *m_fan1 = nullptr;
    DualSpinRow *m_fan2 = nullptr;

    bool m_unlocked = false;
    bool m_connected = false;
};
