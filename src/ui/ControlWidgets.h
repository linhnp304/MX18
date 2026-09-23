#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QLabel;
class QRadioButton;
class QSpinBox;

// Các ô nhập và hàng điều khiển dùng chung cho tab "Điều khiển" (panel 2) và
// các tab của cửa sổ "Điều khiển và thiết lập mức kỹ sư".
//
// Mọi ô nhập đều quy về một giá trị 32 bit đúng như trường trong gói tin
// (raw()/setRaw()), nhờ vậy lớp gọi chỉ phải nối ô nhập với chỉ số trường chứ
// không phải tự đổi đơn vị ở từng chỗ. Ô nhập kiểu float tự lo phép nhân/chia
// theo hệ số của trường đó.
//
// Quy ước hiển thị phản hồi (giống nhau ở cả hai nơi): khi giá trị phản hồi
// khác giá trị đang điều khiển thì ô nhập / ComboBox hiện thêm giá trị phản hồi
// màu đỏ bên cạnh, còn RadioOption thì đổi lựa chọn ứng với giá trị phản hồi
// sang màu đỏ.

class FieldEditor : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    // Giá trị đúng như trường trong gói tin.
    virtual quint32 raw() const = 0;
    // Đặt giá trị mà không phát edited() — dùng khi nạp từ gói phản hồi.
    virtual void setRaw(quint32 v) = 0;
    // differs = false xoá dấu phản hồi; true thì hiện giá trị phản hồi màu đỏ.
    virtual void showFeedback(bool differs, quint32 raw) = 0;

signals:
    void edited();
};

// Ô nhập số nguyên (có dấu được: dải âm truyền vào lo < 0).
class IntEditor : public FieldEditor
{
    Q_OBJECT
public:
    IntEditor(int lo, int hi, QWidget *parent = nullptr);

    quint32 raw() const override;
    void setRaw(quint32 v) override;
    void showFeedback(bool differs, quint32 raw) override;

    void setFieldWidth(int px);

private:
    QSpinBox *m_spin = nullptr;
    QLabel *m_back = nullptr;
    bool m_loading = false;
};

// Ô nhập kiểu float: giá trị trên giao diện nhân với scale mới ra trường gói tin.
// wrapAt > 0 nghĩa là raw bằng đúng wrapAt thì quy về 0 (trường giatri_bu).
class DoubleEditor : public FieldEditor
{
    Q_OBJECT
public:
    DoubleEditor(double lo, double hi, int decimals, double scale, bool signedField,
                 quint32 wrapAt = 0, QWidget *parent = nullptr);

    quint32 raw() const override;
    void setRaw(quint32 v) override;
    void showFeedback(bool differs, quint32 raw) override;

    void setFieldWidth(int px);

private:
    double toGui(quint32 raw) const;

    QDoubleSpinBox *m_spin = nullptr;
    QLabel *m_back = nullptr;
    double m_scale = 1.0;
    bool m_signed = false;
    quint32 m_wrapAt = 0;
    bool m_loading = false;
};

// ComboBox mà giá trị trường chính là chỉ số mục đang chọn.
class ComboEditor : public FieldEditor
{
    Q_OBJECT
public:
    ComboEditor(const QStringList &items, QWidget *parent = nullptr);

    quint32 raw() const override;
    void setRaw(quint32 v) override;
    void showFeedback(bool differs, quint32 raw) override;

    QComboBox *combo() const { return m_combo; }

private:
    QComboBox *m_combo = nullptr;
    QLabel *m_back = nullptr;
    bool m_loading = false;
};

// Một dãy lựa chọn loại trừ nhau; số cột quyết định chỗ gấp dòng.
class RadioEditor : public FieldEditor
{
    Q_OBJECT
public:
    RadioEditor(const QStringList &labels, const QVector<quint32> &values, int columns,
                QWidget *parent = nullptr);

    quint32 raw() const override;
    void setRaw(quint32 v) override;
    void showFeedback(bool differs, quint32 raw) override;

    // Đổi chữ và giá trị của một lựa chọn (mode đổi "5"/"6" theo icode1).
    void setOption(int index, const QString &label, quint32 value);
    int indexOf(quint32 value) const;

private:
    void repaintOptions();

    QVector<QRadioButton *> m_buttons;
    QVector<quint32> m_values;
    quint32 m_value = 0;
    int m_redIndex = -1;
    bool m_loading = false;
};

// Ô đánh dấu: trường mang giá trị 0 hoặc 1.
class CheckEditor : public FieldEditor
{
    Q_OBJECT
public:
    explicit CheckEditor(const QString &text, QWidget *parent = nullptr);

    quint32 raw() const override;
    void setRaw(quint32 v) override;
    void showFeedback(bool differs, quint32 raw) override;

private:
    QCheckBox *m_box = nullptr;
    QLabel *m_back = nullptr;
    bool m_loading = false;
};

// Hàng điều khiển có nhãn ở cột đầu. Các hàng trong cùng một nhóm được căn về
// một bề rộng nhãn để mọi lựa chọn bắt đầu thẳng một cột.
class LabeledRow : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;

    QLabel *titleLabel() const { return m_title; }

    // Căn các hàng trong cùng một nhóm về một bề rộng nhãn; gap là khoảng hở
    // thêm giữa nhãn và điều khiển cho thoáng mắt. Nhóm nào có lựa chọn dài
    // (như "Chế độ phát") thì truyền gap = 0 để khỏi tràn ra ngoài panel.
    static void alignTitles(const QVector<LabeledRow *> &rows, int gap = 12);

protected:
    QLabel *m_title = nullptr;
};

// Hàng gồm nhãn dẫn đầu và một hoặc nhiều ô nhập, mỗi ô có thể kèm nhãn con
// ("Tx1 100%:" "Pha:" [ ] "Biên độ:" [ ]).
class FieldRow : public LabeledRow
{
    Q_OBJECT
public:
    explicit FieldRow(const QString &title, QWidget *parent = nullptr);

    // Trả lại chính editor để lớp gọi nối nó với chỉ số trường của gói tin.
    FieldEditor *add(const QString &caption, FieldEditor *editor);
    // Widget không gắn với trường nào (ComboBox "Chọn tần số", nút bấm…).
    QWidget *addPlain(const QString &caption, QWidget *w);
    // Khoảng hở cố định giữa hai cụm ô nhập trong cùng một hàng.
    void addSpacing(int px);
    void addTail(QWidget *w);          // nhãn đi kèm sát mép phải hàng

signals:
    void edited();                     // phát lại từ mọi ô nhập trong hàng

private:
    QHBoxLayout *m_lay = nullptr;
};

// --------------------------------------------------------------------------
// Ba lớp dưới đây giữ nguyên giao diện lập trình của giai đoạn 2 để tab
// "Điều khiển" trên panel 2 không phải viết lại, chỉ thêm setFeedback().

class RadioRow : public LabeledRow
{
    Q_OBJECT
public:
    RadioRow(const QString &title, const QStringList &labels, const QVector<quint32> &values,
             int columns, QWidget *parent = nullptr);

    quint32 value() const { return m_editor->raw(); }
    void setValue(quint32 v) { m_editor->setRaw(v); }
    void setFeedback(bool differs, quint32 raw) { m_editor->showFeedback(differs, raw); }

    void setOption(int index, const QString &label, quint32 value);
    int indexOf(quint32 value) const;

signals:
    void valueChanged(quint32 value);

private:
    RadioEditor *m_editor = nullptr;
};

class SpinRow : public LabeledRow
{
    Q_OBJECT
public:
    SpinRow(const QString &title, int lo, int hi, QWidget *parent = nullptr);

    quint32 value() const { return m_editor->raw(); }
    void setValue(quint32 v) { m_editor->setRaw(v); }
    void setFeedback(bool differs, quint32 raw) { m_editor->showFeedback(differs, raw); }

signals:
    void valueChanged(quint32 value);

private:
    IntEditor *m_editor = nullptr;
};

// Một nhãn chung, hai ô nhập ("Rẻ quạt 1 (độ):  PV đầu … PV cuối …").
class DualSpinRow : public QWidget
{
    Q_OBJECT
public:
    DualSpinRow(const QString &title, const QString &captionA, const QString &captionB,
                int lo, int hi, QWidget *parent = nullptr);

    quint32 valueA() const { return m_a->raw(); }
    quint32 valueB() const { return m_b->raw(); }
    void setValues(quint32 a, quint32 b);     // không phát valueChanged
    void setFeedback(bool differsA, quint32 rawA, bool differsB, quint32 rawB);

signals:
    void valueChanged();

private:
    IntEditor *m_a = nullptr;
    IntEditor *m_b = nullptr;
};
