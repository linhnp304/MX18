#pragma once

#include <QLayout>
#include <QList>

// Xếp các cụm điều khiển thành hàng, hết chỗ thì xuống dòng.
//
// Cửa sổ ViewIQ phải thu nhỏ được còn một nửa (400 px), mà hàng điều khiển thứ
// hai cần ~720 px mới nằm đủ trên một dòng. Ép vào box layout thì các ô chồng
// lên nhau; ở đây cụm nào không vừa thì xuống dòng, chiều cao tính lại theo bề
// rộng (heightForWidth) nên vùng đồ thị bên dưới tự nhường chỗ.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int hSpacing = 16, int vSpacing = 4);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;

private:
    int arrange(const QRect &rect, bool apply) const;

    QList<QLayoutItem *> m_items;
    int m_hSpacing;
    int m_vSpacing;
};
