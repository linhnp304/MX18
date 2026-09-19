#include "ui/ControlWidgets.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

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

// Mọi ô nhập trong tab "Điều khiển" dùng chung một bề rộng để nhìn đồng đều.
constexpr int kSpinWidth = 96;

QSpinBox *makeSpin(int lo, int hi, QWidget *parent)
{
    auto *s = new QSpinBox(parent);
    s->setRange(lo, hi);
    s->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    s->setFixedWidth(kSpinWidth);
    s->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Bàn phím máy trắc thủ hay bị gõ nhầm; ngăn con lăn chuột đổi giá trị khi
    // người dùng chỉ định cuộn danh sách lệnh.
    s->setFocusPolicy(Qt::StrongFocus);
    return s;
}

} // namespace

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

// -------------------------------------------------------------- RadioRow

RadioRow::RadioRow(const QString &title, const QStringList &labels, const QVector<quint32> &values,
                   int columns, QWidget *parent)
    : LabeledRow(parent)
    , m_values(values)
{
    // Nhãn và các lựa chọn nằm chung một hàng: cột 0 là nhãn, các cột sau là
    // lựa chọn. Hàng nào nhiều lựa chọn quá (mã trả lời M1 có 12) thì phần dư
    // xuống dòng dưới, nhãn vẫn ở dòng đầu.
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 1, 0, 1);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(1);

    m_title = captionLabel(title, this);
    m_title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(m_title, 0, 0);

    auto *group = new QButtonGroup(this);
    const int cols = qMax(1, columns > 0 ? columns : labels.size());
    for (int i = 0; i < labels.size(); ++i) {
        auto *rb = new QRadioButton(labels.at(i), this);
        group->addButton(rb, i);
        grid->addWidget(rb, i / cols, i % cols + 1);
        m_buttons.append(rb);
    }
    grid->setColumnStretch(cols + 1, 1);

    if (!m_values.isEmpty())
        m_value = m_values.first();

    connect(group, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (!on || m_loading || id < 0 || id >= m_values.size())
            return;
        m_value = m_values.at(id);
        emit valueChanged(m_value);
    });
}

int RadioRow::indexOf(quint32 value) const
{
    return int(m_values.indexOf(value));
}

void RadioRow::setValue(quint32 v)
{
    m_value = v;
    const int idx = indexOf(v);
    m_loading = true;
    if (idx >= 0 && idx < m_buttons.size())
        m_buttons.at(idx)->setChecked(true);
    m_loading = false;
}

void RadioRow::setOption(int index, const QString &label, quint32 value)
{
    if (index < 0 || index >= m_buttons.size())
        return;
    m_buttons.at(index)->setText(label);
    const bool wasSelected = m_buttons.at(index)->isChecked();
    m_values[index] = value;
    if (wasSelected)
        m_value = value;
}

// --------------------------------------------------------------- SpinRow

SpinRow::SpinRow(const QString &title, int lo, int hi, QWidget *parent)
    : LabeledRow(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 1, 0, 1);
    lay->setSpacing(4);

    m_title = captionLabel(title, this);
    lay->addWidget(m_title);
    m_spin = makeSpin(lo, hi, this);
    // Ô nhập bắt đầu ngay sau nhãn giống các lựa chọn của RadioRow, chỗ thừa
    // dồn về bên phải chứ không kéo giãn ô nhập ra hết hàng.
    lay->addWidget(m_spin);
    lay->addStretch(1);

    connect(m_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        if (!m_loading)
            emit valueChanged(quint32(v));
    });
}

quint32 SpinRow::value() const
{
    return quint32(m_spin->value());
}

void SpinRow::setValue(quint32 v)
{
    m_loading = true;
    m_spin->setValue(int(v));
    m_loading = false;
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
    m_a = makeSpin(lo, hi, this);
    m_b = makeSpin(lo, hi, this);
    row->addWidget(captionLabel(captionA, this));
    row->addWidget(m_a);
    row->addWidget(captionLabel(captionB, this));
    row->addWidget(m_b);
    row->addStretch(1);
    lay->addLayout(row);

    const auto onEdit = [this] {
        if (!m_loading)
            emit valueChanged();
    };
    connect(m_a, QOverload<int>::of(&QSpinBox::valueChanged), this, onEdit);
    connect(m_b, QOverload<int>::of(&QSpinBox::valueChanged), this, onEdit);
}

quint32 DualSpinRow::valueA() const { return quint32(m_a->value()); }
quint32 DualSpinRow::valueB() const { return quint32(m_b->value()); }

void DualSpinRow::setValues(quint32 a, quint32 b)
{
    m_loading = true;
    m_a->setValue(int(a));
    m_b->setValue(int(b));
    m_loading = false;
}
