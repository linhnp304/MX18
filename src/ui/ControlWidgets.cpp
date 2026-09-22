#include "ui/ControlWidgets.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace {

// Màu chữ của giá trị phản hồi khác giá trị đang điều khiển.
const char *const kBackStyle = "color:#ff4d4d;font-weight:bold;";

QLabel *captionLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    // Đặt màu bằng palette chứ không bằng stylesheet: stylesheet ghi đè cả
    // trạng thái disabled nên nhãn của điều khiển bị khoá vẫn sáng như thường.
    QPalette pal = l->palette();
    pal.setColor(QPalette::WindowText, QColor(0xb9, 0xc3, 0xcd));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5d, 0x66, 0x6f));
    l->setPalette(pal);
    return l;
}

// Nhãn hiện giá trị phản hồi; ẩn khi hai bên khớp nhau.
QLabel *backLabel(QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setStyleSheet(QString::fromLatin1(kBackStyle));
    l->hide();
    return l;
}

// Mọi ô nhập dùng chung một bề rộng để nhìn đồng đều.
constexpr int kSpinWidth = 96;

void styleSpin(QAbstractSpinBox *s)
{
    s->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    s->setFixedWidth(kSpinWidth);
    s->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Bàn phím máy trắc thủ hay bị gõ nhầm; ngăn con lăn chuột đổi giá trị khi
    // người dùng chỉ định cuộn danh sách lệnh.
    s->setFocusPolicy(Qt::StrongFocus);
}

// Bỏ số 0 thừa ở đuôi cho nhãn phản hồi đỡ rối ("1,005" chứ không "1,00500").
QString trimZeros(double v, int decimals)
{
    QString t = QString::number(v, 'f', decimals);
    if (t.contains(QLatin1Char('.'))) {
        while (t.endsWith(QLatin1Char('0')))
            t.chop(1);
        if (t.endsWith(QLatin1Char('.')))
            t.chop(1);
    }
    return t;
}

} // namespace

// ------------------------------------------------------------- IntEditor

IntEditor::IntEditor(int lo, int hi, QWidget *parent)
    : FieldEditor(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    m_spin = new QSpinBox(this);
    m_spin->setRange(lo, hi);
    styleSpin(m_spin);
    m_back = backLabel(this);

    lay->addWidget(m_spin);
    lay->addWidget(m_back);

    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (!m_loading)
            emit edited();
    });
}

void IntEditor::setFieldWidth(int px)
{
    m_spin->setFixedWidth(px);
}

quint32 IntEditor::raw() const
{
    return quint32(qint32(m_spin->value()));
}

void IntEditor::setRaw(quint32 v)
{
    m_loading = true;
    m_spin->setValue(int(qint32(v)));
    m_loading = false;
}

void IntEditor::showFeedback(bool differs, quint32 raw)
{
    m_back->setVisible(differs);
    if (differs)
        m_back->setText(QString::number(qint32(raw)));
}

// ---------------------------------------------------------- DoubleEditor

DoubleEditor::DoubleEditor(double lo, double hi, int decimals, double scale, bool signedField,
                           quint32 wrapAt, QWidget *parent)
    : FieldEditor(parent)
    , m_scale(scale)
    , m_signed(signedField)
    , m_wrapAt(wrapAt)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    m_spin = new QDoubleSpinBox(this);
    m_spin->setDecimals(decimals);
    m_spin->setRange(lo, hi);
    m_spin->setSingleStep(std::pow(10.0, -decimals));
    styleSpin(m_spin);
    m_back = backLabel(this);

    lay->addWidget(m_spin);
    lay->addWidget(m_back);

    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this] {
        if (!m_loading)
            emit edited();
    });
}

void DoubleEditor::setFieldWidth(int px)
{
    m_spin->setFixedWidth(px);
}

quint32 DoubleEditor::raw() const
{
    const double scaled = std::round(m_spin->value() * m_scale);
    const quint32 v = m_signed ? quint32(qint32(scaled)) : quint32(qint64(scaled));
    // Trường giatri_bu: 360 độ tròn vòng quy về 0 chứ không tràn lên 4096.
    return (m_wrapAt > 0 && v == m_wrapAt) ? 0u : v;
}

double DoubleEditor::toGui(quint32 raw) const
{
    const double n = m_signed ? double(qint32(raw)) : double(raw);
    return n / m_scale;
}

void DoubleEditor::setRaw(quint32 v)
{
    m_loading = true;
    m_spin->setValue(toGui(v));
    m_loading = false;
}

void DoubleEditor::showFeedback(bool differs, quint32 raw)
{
    m_back->setVisible(differs);
    if (differs)
        m_back->setText(trimZeros(toGui(raw), m_spin->decimals()));
}

// ----------------------------------------------------------- ComboEditor

ComboEditor::ComboEditor(const QStringList &items, QWidget *parent)
    : FieldEditor(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    m_combo = new QComboBox(this);
    m_combo->addItems(items);
    m_combo->setFocusPolicy(Qt::StrongFocus);
    // Rộng bằng ô nhập số cho các hàng nhìn đều nhau, và đủ chỗ cho mũi tên.
    m_combo->setMinimumWidth(kSpinWidth);
    m_back = backLabel(this);

    lay->addWidget(m_combo);
    lay->addWidget(m_back);

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (!m_loading)
            emit edited();
    });
}

quint32 ComboEditor::raw() const
{
    return quint32(qMax(0, m_combo->currentIndex()));
}

void ComboEditor::setRaw(quint32 v)
{
    m_loading = true;
    m_combo->setCurrentIndex(qBound(0, int(v), m_combo->count() - 1));
    m_loading = false;
}

void ComboEditor::showFeedback(bool differs, quint32 raw)
{
    m_back->setVisible(differs);
    if (!differs)
        return;
    const int idx = int(raw);
    m_back->setText(idx >= 0 && idx < m_combo->count() ? m_combo->itemText(idx)
                                                       : QString::number(raw));
}

// ----------------------------------------------------------- RadioEditor

RadioEditor::RadioEditor(const QStringList &labels, const QVector<quint32> &values, int columns,
                         QWidget *parent)
    : FieldEditor(parent)
    , m_values(values)
{
    // Hàng nào nhiều lựa chọn quá (mã trả lời M1 có 12) thì phần dư xuống dòng
    // dưới; số cột do lớp gọi quyết định.
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(1);

    auto *group = new QButtonGroup(this);
    const int cols = qMax(1, columns > 0 ? columns : labels.size());
    for (int i = 0; i < labels.size(); ++i) {
        auto *rb = new QRadioButton(labels.at(i), this);
        group->addButton(rb, i);
        grid->addWidget(rb, i / cols, i % cols);
        m_buttons.append(rb);
    }

    if (!m_values.isEmpty())
        m_value = m_values.first();

    connect(group, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (!on || m_loading || id < 0 || id >= m_values.size())
            return;
        m_value = m_values.at(id);
        emit edited();
    });
}

int RadioEditor::indexOf(quint32 value) const
{
    return int(m_values.indexOf(value));
}

quint32 RadioEditor::raw() const
{
    return m_value;
}

void RadioEditor::setRaw(quint32 v)
{
    m_value = v;
    const int idx = indexOf(v);
    m_loading = true;
    if (idx >= 0 && idx < m_buttons.size())
        m_buttons.at(idx)->setChecked(true);
    m_loading = false;
}

void RadioEditor::setOption(int index, const QString &label, quint32 value)
{
    if (index < 0 || index >= m_buttons.size())
        return;
    m_buttons.at(index)->setText(label);
    const bool wasSelected = m_buttons.at(index)->isChecked();
    m_values[index] = value;
    if (wasSelected)
        m_value = value;
}

void RadioEditor::showFeedback(bool differs, quint32 raw)
{
    m_redIndex = differs ? indexOf(raw) : -1;
    repaintOptions();
}

void RadioEditor::repaintOptions()
{
    // Phải đổi màu bằng stylesheet chứ không bằng palette: bảng kiểu chung của
    // phần mềm có luật "QWidget { color: ... }" nên palette của QRadioButton
    // không còn tác dụng. Kèm luôn luật :disabled, nếu không lựa chọn bị khoá
    // vẫn sáng nguyên như đang bấm được.
    static const QString red = QStringLiteral(
        "QRadioButton { color:#ff4d4d; } QRadioButton:disabled { color:#a83a3a; }");
    for (int i = 0; i < m_buttons.size(); ++i) {
        QRadioButton *rb = m_buttons.at(i);
        const QString want = (i == m_redIndex) ? red : QString();
        if (rb->styleSheet() != want)
            rb->setStyleSheet(want);
    }
}

// ----------------------------------------------------------- CheckEditor

CheckEditor::CheckEditor(const QString &text, QWidget *parent)
    : FieldEditor(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    m_box = new QCheckBox(text, this);
    m_back = backLabel(this);
    lay->addWidget(m_box);
    lay->addWidget(m_back);

    connect(m_box, &QCheckBox::toggled, this, [this] {
        if (!m_loading)
            emit edited();
    });
}

quint32 CheckEditor::raw() const
{
    return m_box->isChecked() ? 1u : 0u;
}

void CheckEditor::setRaw(quint32 v)
{
    m_loading = true;
    m_box->setChecked(v != 0);
    m_loading = false;
}

void CheckEditor::showFeedback(bool differs, quint32 raw)
{
    m_back->setVisible(differs);
    if (differs)
        m_back->setText(raw != 0 ? QStringLiteral("Bật") : QStringLiteral("Tắt"));
}

// ------------------------------------------------------------- LabeledRow

void LabeledRow::alignTitles(const QVector<LabeledRow *> &rows, int gap)
{
    int width = 0;
    for (LabeledRow *r : rows) {
        if (r && r->m_title)
            width = qMax(width, r->m_title->sizeHint().width());
    }
    width += qMax(0, gap);
    for (LabeledRow *r : rows) {
        if (r && r->m_title)
            r->m_title->setMinimumWidth(width);
    }
}

// --------------------------------------------------------------- FieldRow

FieldRow::FieldRow(const QString &title, QWidget *parent)
    : LabeledRow(parent)
{
    m_lay = new QHBoxLayout(this);
    m_lay->setContentsMargins(0, 1, 0, 1);
    m_lay->setSpacing(4);

    m_title = captionLabel(title, this);
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lay->addWidget(m_title);
    m_lay->addStretch(1);
}

FieldEditor *FieldRow::add(const QString &caption, FieldEditor *editor)
{
    // Chèn trước phần co giãn cuối hàng để chỗ thừa luôn dồn về bên phải.
    const int at = m_lay->count() - 1;
    if (!caption.isEmpty())
        m_lay->insertWidget(at, captionLabel(caption, this));
    m_lay->insertWidget(m_lay->count() - 1, editor);
    connect(editor, &FieldEditor::edited, this, &FieldRow::edited);
    return editor;
}

QWidget *FieldRow::addPlain(const QString &caption, QWidget *w)
{
    const int at = m_lay->count() - 1;
    if (!caption.isEmpty())
        m_lay->insertWidget(at, captionLabel(caption, this));
    m_lay->insertWidget(m_lay->count() - 1, w);
    return w;
}

void FieldRow::addTail(QWidget *w)
{
    m_lay->addWidget(w);
}

// --------------------------------------------------------------- RadioRow

RadioRow::RadioRow(const QString &title, const QStringList &labels, const QVector<quint32> &values,
                   int columns, QWidget *parent)
    : LabeledRow(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 1, 0, 1);
    lay->setSpacing(4);

    m_title = captionLabel(title, this);
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lay->addWidget(m_title);

    m_editor = new RadioEditor(labels, values, columns, this);
    lay->addWidget(m_editor);
    lay->addStretch(1);

    connect(m_editor, &FieldEditor::edited, this,
            [this] { emit valueChanged(m_editor->raw()); });
}

void RadioRow::setOption(int index, const QString &label, quint32 value)
{
    m_editor->setOption(index, label, value);
}

int RadioRow::indexOf(quint32 value) const
{
    return m_editor->indexOf(value);
}

// ---------------------------------------------------------------- SpinRow

SpinRow::SpinRow(const QString &title, int lo, int hi, QWidget *parent)
    : LabeledRow(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 1, 0, 1);
    lay->setSpacing(4);

    m_title = captionLabel(title, this);
    lay->addWidget(m_title);

    // Ô nhập bắt đầu ngay sau nhãn giống các lựa chọn của RadioRow, chỗ thừa
    // dồn về bên phải chứ không kéo giãn ô nhập ra hết hàng.
    m_editor = new IntEditor(lo, hi, this);
    lay->addWidget(m_editor);
    lay->addStretch(1);

    connect(m_editor, &FieldEditor::edited, this,
            [this] { emit valueChanged(m_editor->raw()); });
}

// ----------------------------------------------------------- DualSpinRow

DualSpinRow::DualSpinRow(const QString &title, const QString &captionA, const QString &captionB,
                         int lo, int hi, QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 1, 0, 1);
    lay->setSpacing(1);
    lay->addWidget(captionLabel(title, this));

    auto *row = new QHBoxLayout;
    row->setContentsMargins(10, 0, 0, 0);
    row->setSpacing(4);
    m_a = new IntEditor(lo, hi, this);
    m_b = new IntEditor(lo, hi, this);
    row->addWidget(captionLabel(captionA, this));
    row->addWidget(m_a);
    row->addWidget(captionLabel(captionB, this));
    row->addWidget(m_b);
    row->addStretch(1);
    lay->addLayout(row);

    connect(m_a, &FieldEditor::edited, this, &DualSpinRow::valueChanged);
    connect(m_b, &FieldEditor::edited, this, &DualSpinRow::valueChanged);
}

void DualSpinRow::setValues(quint32 a, quint32 b)
{
    m_a->setRaw(a);
    m_b->setRaw(b);
}

void DualSpinRow::setFeedback(bool differsA, quint32 rawA, bool differsB, quint32 rawB)
{
    m_a->showFeedback(differsA, rawA);
    m_b->showFeedback(differsB, rawB);
}
