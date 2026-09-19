#pragma once

#include "core/Settings.h"
#include "net/LinkConfig.h"

#include <QWidget>

class QCheckBox;
class QTableWidget;
class QTabWidget;

// Cửa sổ "Điều khiển và thiết lập mức kỹ sư".
//
// Là cửa sổ Qt::Tool chứ không phải hộp thoại chặn: luôn nổi trên giao diện
// chính nhưng trắc thủ vẫn bấm được vào giao diện chính, và di chuyển được.
//
// Giai đoạn này mới làm tab "Connect"; Admin/AD/SW/Other/Params dựng sẵn khung.
class EngineerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit EngineerWindow(QWidget *parent = nullptr);

    // Đúng một lần mỗi lần chạy phần mềm mới phải nhập mật khẩu kỹ sư.
    static bool passwordAccepted();
    static void rememberPassword();

signals:
    void configSaved(const QString &message);

private:
    QWidget *buildConnectTab();
    QWidget *buildPlaceholderTab(const QString &note);

    void fillLinkTable();
    void fillNodeTable();
    void saveLinks();
    void saveNodes();

    void setNodeRow(int row, const NetNode &node);
    void fitTable(QTableWidget *table, int visibleRows);
    QWidget *makeIpEdit(const QString &value);
    QWidget *makePortSpin(quint16 value);

    QTabWidget *m_tabs = nullptr;
    QCheckBox *m_lockBox = nullptr;
    QTableWidget *m_links = nullptr;
    QTableWidget *m_nodes = nullptr;
    QVector<QWidget *> m_lockedTabs;   // Admin, AD, SW, Other
};
