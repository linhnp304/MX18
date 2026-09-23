#pragma once

#include "core/Settings.h"
#include "net/LinkConfig.h"

#include <QVector>
#include <QWidget>

class AdminTab;
class AdTab;
class CommandBlock;
class EngineerTab;
class OtherTab;
class ParamsTab;
class SwTab;
class QCheckBox;
class QLabel;
class QTableWidget;
class QTabWidget;

// Cửa sổ "Điều khiển và thiết lập mức kỹ sư".
//
// Là cửa sổ Qt::Tool chứ không phải hộp thoại chặn: luôn nổi trên giao diện
// chính nhưng trắc thủ vẫn bấm được vào giao diện chính, và di chuyển được.
//
// Ô "Khóa điều khiển" ở góc dưới bên trái khoá cả năm tab lệnh cùng lúc; lúc
// khoá thì các ô nhập bám theo trạng thái phản hồi của hệ thống MH, lúc mở khoá
// thì giữ giá trị kỹ sư đang đặt và đánh dấu đỏ chỗ lệch. Mỗi lần mở cửa sổ ô
// này đều bắt đầu ở trạng thái khoá.
class EngineerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit EngineerWindow(QWidget *parent = nullptr);

    // Đúng một lần mỗi lần chạy phần mềm mới phải nhập mật khẩu kỹ sư.
    static bool passwordAccepted();
    static void rememberPassword();

    // Gói tin phản hồi / trạng thái từ hệ thống MH về đúng tab của nó.
    void applyFrame(quint32 category, quint32 serial, const quint32 *fields, int count);
    // Gửi lệnh thành công: hiện serial của gói vừa gửi.
    void noteSent(quint32 category, quint32 serial);
    // Dừng kết nối: xoá mọi dấu phản hồi đang hiện.
    void clearBack();
    // Bật/tắt ô "Khóa điều khiển" từ bên ngoài (sau lệnh khởi động lại MH).
    void setLocked(bool locked);

signals:
    void configSaved(const QString &message);
    // Lớp trên đóng gói theo thứ tự byte của connect.json rồi gửi qua Cmd-Admin.
    void commandReady(quint32 category, const QVector<quint32> &fields);
    void rebootRequested();
    void viewIqRequested();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QWidget *buildConnectTab();

    void wireBlock(CommandBlock *block);
    void updateCornerSerial();

    void fillLinkTable();
    void fillNodeTable();
    void saveLinks();
    void saveNodes();

    void setNodeRow(int row, const NetNode &node);
    void fitTable(QTableWidget *table, int visibleRows);
    void relaxTable(QTableWidget *table);
    QWidget *makeIpEdit(const QString &value);
    QWidget *makePortSpin(quint16 value);

    QTabWidget *m_tabs = nullptr;
    QCheckBox *m_lockBox = nullptr;
    QLabel *m_cornerSerial = nullptr;
    QTableWidget *m_links = nullptr;
    QTableWidget *m_nodes = nullptr;

    AdminTab *m_adminTab = nullptr;
    AdTab *m_adTab = nullptr;
    SwTab *m_swTab = nullptr;
    OtherTab *m_otherTab = nullptr;
    ParamsTab *m_paramsTab = nullptr;
    QVector<EngineerTab *> m_commandTabs;
    QVector<CommandBlock *> m_blocks;
};
