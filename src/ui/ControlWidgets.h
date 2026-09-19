#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QRadioButton;
class QSpinBox;

// Các hàng điều khiển dùng lại cho tab "Điều khiển" và cửa sổ mức kỹ sư.
//
// Panel 2 chỉ rộng chừng 250–320 px nên nhãn đặt trên một dòng riêng, lựa chọn
// xếp lưới bên dưới; số cột do lớp gọi quyết định để hàng 12 lựa chọn (rcode1)
// gấp thành 2 dòng theo đúng đặc tả.

// Hàng điều khiển có nhãn ở cột đầu. Các hàng trong cùng một nhóm được căn về
// một bề rộng nhãn để mọi lựa chọn bắt đầu thẳng một cột.
class LabeledRow : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    QLabel *titleLabel() const { return m_title; }
    static void alignTitles(const QVector<LabeledRow *> &rows);

protected:
    QLabel *m_title = nullptr;
};

class RadioRow : public LabeledRow
{
    Q_OBJECT
public:
    RadioRow(const QString &title, const QStringList &labels, const QVector<quint32> &values,
             int columns, QWidget *parent = nullptr);

    quint32 value() const { return m_value; }
    void setValue(quint32 v);                 // không phát valueChanged

    // Đổi chữ và giá trị của một lựa chọn (mode đổi "5"/"6" theo icode1).
    void setOption(int index, const QString &label, quint32 value);
    int indexOf(quint32 value) const;

signals:
    void valueChanged(quint32 value);

private:
    QVector<QRadioButton *> m_buttons;
    QVector<quint32> m_values;
    quint32 m_value = 0;
    bool m_loading = false;
};

// Nhãn + ô nhập số nguyên.
class SpinRow : public LabeledRow
{
    Q_OBJECT
public:
    SpinRow(const QString &title, int lo, int hi, QWidget *parent = nullptr);

    quint32 value() const;
    void setValue(quint32 v);                 // không phát valueChanged

signals:
    void valueChanged(quint32 value);

private:
    QSpinBox *m_spin = nullptr;
    bool m_loading = false;
};

// Một nhãn chung, hai ô nhập ("Rẻ quạt 1 (độ):  PV đầu … PV cuối …").
class DualSpinRow : public QWidget
{
    Q_OBJECT
public:
    DualSpinRow(const QString &title, const QString &captionA, const QString &captionB,
                int lo, int hi, QWidget *parent = nullptr);

    quint32 valueA() const;
    quint32 valueB() const;
    void setValues(quint32 a, quint32 b);     // không phát valueChanged

signals:
    void valueChanged();

private:
    QSpinBox *m_a = nullptr;
    QSpinBox *m_b = nullptr;
    bool m_loading = false;
};
