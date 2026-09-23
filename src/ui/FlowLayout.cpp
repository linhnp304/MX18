#include "ui/FlowLayout.h"

#include <QWidget>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

FlowLayout::FlowLayout(QWidget *parent, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpacing(hSpacing)
    , m_vSpacing(vSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
}

int FlowLayout::count() const
{
    return int(m_items.size());
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return (index >= 0 && index < m_items.size()) ? m_items.at(index) : nullptr;
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

int FlowLayout::heightForWidth(int width) const
{
    return arrange(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::minimumSize() const
{
    // Hẹp nhất là bề rộng của cụm lớn nhất: mỗi cụm một dòng.
    QSize size;
    for (const QLayoutItem *item : m_items)
        size = size.expandedTo(item->minimumSize());
    const QMargins m = contentsMargins();
    return size + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
    // Kích thước mong muốn là tất cả trên một dòng.
    int width = 0;
    int height = 0;
    for (const QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        const QSize s = item->sizeHint();
        width += s.width() + (width > 0 ? m_hSpacing : 0);
        height = qMax(height, s.height());
    }
    const QMargins m = contentsMargins();
    return QSize(width + m.left() + m.right(), height + m.top() + m.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int FlowLayout::arrange(const QRect &rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.marginsRemoved(m);
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;

    // Các cụm trên cùng một dòng căn giữa theo chiều dọc, nên phải biết chiều
    // cao của dòng trước khi đặt: gom từng dòng rồi mới đặt.
    QList<QLayoutItem *> line;
    const auto flush = [&] {
        if (apply) {
            int lx = area.x();
            for (QLayoutItem *it : std::as_const(line)) {
                const QSize s = it->sizeHint();
                it->setGeometry(QRect(QPoint(lx, y + (lineHeight - s.height()) / 2), s));
                lx += s.width() + m_hSpacing;
            }
        }
        line.clear();
    };

    for (QLayoutItem *item : m_items) {
        if (item->isEmpty())
            continue;
        const QSize s = item->sizeHint();
        if (!line.isEmpty() && x + s.width() > area.right() + 1) {
            flush();
            x = area.x();
            y += lineHeight + m_vSpacing;
            lineHeight = 0;
        }
        line.append(item);
        x += s.width() + m_hSpacing;
        lineHeight = qMax(lineHeight, s.height());
    }
    flush();
    return y + lineHeight - rect.y() + m.bottom();
}
