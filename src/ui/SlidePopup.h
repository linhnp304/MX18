#pragma once

#include <QFrame>

class QLabel;
class QPropertyAnimation;
class QVBoxLayout;

// Cửa sổ popup của thanh trạng thái.
//
// Là widget con của cửa sổ chính (không phải cửa sổ hệ điều hành) để hiệu ứng
// trượt luôn nằm gọn trong khung phần mềm và không nhảy ra ngoài màn hình.
class SlidePopup : public QFrame
{
    Q_OBJECT
public:
    enum Side { FromLeft, FromRight };

    SlidePopup(const QString &title, Side side, QWidget *parent);

    QWidget *body() const { return m_body; }
    bool isOpen() const { return m_open; }

    // targetTopLeft tính theo toạ độ của widget cha.
    void openAt(const QPoint &targetTopLeft);
    void closePopup();

signals:
    void closed();

private:
    QWidget *m_body = nullptr;
    QPropertyAnimation *m_anim = nullptr;
    Side m_side;
    bool m_open = false;
    QPoint m_target;
};
