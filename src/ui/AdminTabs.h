#pragma once

#include "proto/Packets.h"

#include <QVector>
#include <QWidget>

class FieldEditor;
class FieldRow;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;

// Các tab điều khiển của cửa sổ "Điều khiển và thiết lập mức kỹ sư".
//
// Khác tab "Điều khiển" trên panel 2 ở một điểm: lệnh chỉ đi khi kỹ sư bấm nút
// "Gửi lệnh", chứ không phải hễ đổi một điều khiển là gửi. Vì vậy mỗi gói lệnh
// được gói vào một CommandBlock giữ luôn nút bấm và hai nhãn serial.

// ------------------------------------------------------------ CommandBlock

class CommandBlock : public QObject
{
    Q_OBJECT
public:
    CommandBlock(quint32 category, quint32 backCategory, int count, const quint32 *defaults,
                 QObject *parent);

    quint32 category() const { return m_category; }
    quint32 backCategory() const { return m_backCategory; }

    // Nối một ô nhập vào một trường của gói lệnh.
    FieldEditor *bind(FieldEditor *editor, int field);
    // Widget bị vô hiệu khi bật "Khóa điều khiển" (nhóm điều khiển, nút gửi…).
    void addLocked(QWidget *w);
    // packetName hiện trong tooltip: cửa sổ có sáu nút "Gửi lệnh" giống hệt nhau.
    void setSendButton(QPushButton *btn, const QString &packetName);
    void setSerialLabels(QLabel *sent, QLabel *back);

    quint32 field(int i) const { return m_fields.at(i); }
    void setField(int i, quint32 v);          // cập nhật cả ô nhập tương ứng
    const QVector<quint32> &fields() const { return m_fields; }

    bool hasBack() const { return m_hasBack; }
    quint32 backField(int i) const { return m_back.at(i); }
    QString backSerialText() const;

    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    void applyBack(const quint32 *fields, quint32 serial);
    void noteSent(quint32 serial);
    void clearBack();

signals:
    void sendRequested();
    void fieldEdited(int field, quint32 value);
    void backReceived();                       // vừa nhận một gói phản hồi
    void sent();                               // gửi lệnh thành công
    void backSerialChanged();

private:
    void refresh();

    struct Bound {
        FieldEditor *editor;
        int field;
    };

    quint32 m_category;
    quint32 m_backCategory;
    QVector<quint32> m_fields;
    QVector<quint32> m_back;
    QVector<Bound> m_bound;
    QVector<QWidget *> m_lockedWidgets;
    QLabel *m_serialSent = nullptr;
    QLabel *m_serialBack = nullptr;
    quint32 m_backSerial = 0;
    bool m_hasBack = false;
    bool m_hasBackSerial = false;
    bool m_locked = true;
};

// --------------------------------------------------------------- lớp chung

class EngineerTab : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    // Nhãn serial của gói phản hồi, hiện ở góc phải thanh tab.
    virtual QString cornerSerialText() const { return QString(); }
    virtual void setLocked(bool locked) { Q_UNUSED(locked); }
    virtual void clearBack() {}

signals:
    void cornerSerialChanged();
};

// ---------------------------------------------------------------- tab ADMIN

class AdminTab : public EngineerTab
{
    Q_OBJECT
public:
    explicit AdminTab(QWidget *parent = nullptr);

    CommandBlock *block() const { return m_block; }
    QString cornerSerialText() const override;
    void setLocked(bool locked) override;
    void clearBack() override;

    // Bấm "Gửi lệnh" lúc cờ này đang dựng thì phải gửi CMD_ADMIN_AD trước.
    bool needsAdFirst() const { return m_flagSendAd; }

    void applyCalibStatus(const quint32 *fields, quint32 serial);

signals:
    // calib_onoff vừa đổi so với lệnh gửi trước đó: tab "AD" nhảy sang đúng
    // mục "Chọn tần số" tương ứng.
    void calibPresetRequested(int presetIndex);

private:
    QGroupBox *buildVideoGroup();
    QGroupBox *buildTxGroup();
    QGroupBox *buildAkGroup();
    QGroupBox *buildCalibGroup();
    QGroupBox *buildCalibResultGroup();

    void updateDeltaEnabled();
    void resetCalibTracking();

    CommandBlock *m_block = nullptr;
    QPushButton *m_viewIqBtn = nullptr;
    FieldEditor *m_deltaF2 = nullptr;
    FieldEditor *m_deltaF3 = nullptr;
    FieldEditor *m_deltaF4 = nullptr;

    QTableWidget *m_calibTable = nullptr;
    QLabel *m_calibSerial = nullptr;

    quint32 m_lastSentCalibOnoff = 0;
    bool m_flagSendAd = false;

    // Theo dõi biên độ Tx2 nhỏ nhất trong lúc hiệu chuẩn System-F4; đặt lại mỗi
    // khi gửi CMD_ADMIN thành công.
    int m_deltaTxCount = 0;
    quint32 m_minAmp = 0;
    double m_minDelta = 0.0;
    bool m_hasMin = false;
};

// ------------------------------------------------------------------- tab AD

class AdTab : public EngineerTab
{
    Q_OBJECT
public:
    explicit AdTab(QWidget *parent = nullptr);

    CommandBlock *block() const { return m_block; }
    QString cornerSerialText() const override;
    void setLocked(bool locked) override;
    void clearBack() override;

public slots:
    // Chọn một mục trong "Chọn tần số" và nạp cặp tần số của mục đó.
    void selectPreset(int index);

private:
    void applyPreset(int index);

    CommandBlock *m_block = nullptr;
    QComboBox *m_preset = nullptr;
};

// ------------------------------------------------------------------- tab SW

class SwTab : public EngineerTab
{
    Q_OBJECT
public:
    explicit SwTab(QWidget *parent = nullptr);

    CommandBlock *block() const { return m_block; }
    QString cornerSerialText() const override;
    void setLocked(bool locked) override;
    void clearBack() override;

private:
    CommandBlock *m_block = nullptr;
};

// ---------------------------------------------------------------- tab Other

class OtherTab : public EngineerTab
{
    Q_OBJECT
public:
    explicit OtherTab(QWidget *parent = nullptr);

    CommandBlock *other() const { return m_other; }
    CommandBlock *calibReg() const { return m_calibReg; }
    CommandBlock *buphabd() const { return m_buphabd; }

    void setLocked(bool locked) override;
    void clearBack() override;

signals:
    void rebootRequested();

private:
    QGroupBox *buildOtherGroup();
    QGroupBox *buildCalibRegGroup();
    QGroupBox *buildBuPhaGroup();

    CommandBlock *m_other = nullptr;
    CommandBlock *m_calibReg = nullptr;
    CommandBlock *m_buphabd = nullptr;
    QPushButton *m_rebootBtn = nullptr;
};

// --------------------------------------------------------------- tab Params

class ParamsTab : public EngineerTab
{
    Q_OBJECT
public:
    explicit ParamsTab(QWidget *parent = nullptr);

    QString cornerSerialText() const override;
    void clearBack() override;

    void applyParams(const quint32 *fields, quint32 serial);

private:
    QTableWidget *m_table = nullptr;
    quint32 m_serial = 0;
    bool m_hasSerial = false;
};
