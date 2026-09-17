#pragma once

#include <QVector>
#include <QWidget>

class QGroupBox;
class QPushButton;

// Tab "Điều khiển" của panel 2.
//
// Hai mức điều khiển: mức người dùng nằm ngay trên tab này, mức kỹ sư nằm trong
// cửa sổ riêng sau lớp mật khẩu. Nội dung chi tiết từng nhóm lệnh sẽ bổ sung ở
// giai đoạn sau; khung nhóm và cơ chế khoá/mở khoá dựng sẵn từ giai đoạn này.
class ControlTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlTab(QWidget *parent = nullptr);

    bool isUnlocked() const { return m_unlocked; }
    void setUnlocked(bool unlocked);

signals:
    void engineerRequested();
    void lockChanged(bool unlocked);

private:
    QGroupBox *addGroup(const QString &title, QWidget *parent);

    QPushButton *m_lockBtn = nullptr;
    QPushButton *m_engineerBtn = nullptr;
    QVector<QGroupBox *> m_groups;
    bool m_unlocked = false;
};
