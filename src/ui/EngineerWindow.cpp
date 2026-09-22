#include "ui/EngineerWindow.h"

#include "proto/Dataframe.h"
#include "ui/AdminTabs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QScrollBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

// Nhập đúng mật khẩu kỹ sư một lần là đủ cho cả lần chạy phần mềm.
bool g_passwordAccepted = false;

const QStringList kDirNames = {QStringLiteral("Recv"), QStringLiteral("Send"),
                               QStringLiteral("Send/Recv")};
const QStringList kProtoNames = {QStringLiteral("UDP"), QStringLiteral("TCP")};
const QStringList kUdpTypes = {QStringLiteral("Unicast"), QStringLiteral("Broadcast")};
const QStringList kTcpTypes = {QStringLiteral("Server"), QStringLiteral("Client")};
const QStringList kNodeKinds = {QStringLiteral("Không cảnh báo"), QStringLiteral("Cảnh báo"),
                                QStringLiteral("Báo lỗi")};

QComboBox *makeCombo(const QStringList &items, int current)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setCurrentIndex(qBound(0, current, items.size() - 1));
    return c;
}

QValidator *ipValidator(QObject *parent)
{
    // Bốn nhóm 0..255; QRegularExpressionValidator chặn ngay lúc gõ nên kỹ sư
    // không lưu được địa chỉ sai cú pháp.
    static const QString octet = QStringLiteral("(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])");
    const QRegularExpression re(QStringLiteral("^%1\\.%1\\.%1\\.%1$").arg(octet));
    return new QRegularExpressionValidator(re, parent);
}

QString comboText(QTableWidget *t, int row, int col)
{
    auto *c = qobject_cast<QComboBox *>(t->cellWidget(row, col));
    return c ? c->currentText() : QString();
}

int comboIndex(QTableWidget *t, int row, int col)
{
    auto *c = qobject_cast<QComboBox *>(t->cellWidget(row, col));
    return c ? c->currentIndex() : 0;
}

QString editText(QTableWidget *t, int row, int col)
{
    auto *e = qobject_cast<QLineEdit *>(t->cellWidget(row, col));
    return e ? e->text().trimmed() : QString();
}

quint16 spinValue(QTableWidget *t, int row, int col)
{
    auto *s = qobject_cast<QSpinBox *>(t->cellWidget(row, col));
    return s ? quint16(s->value()) : quint16(0);
}

} // namespace

bool EngineerWindow::passwordAccepted() { return g_passwordAccepted; }
void EngineerWindow::rememberPassword() { g_passwordAccepted = true; }

EngineerWindow::EngineerWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                          | Qt::WindowStaysOnTopHint)
{
    setWindowTitle(QStringLiteral("Điều khiển và thiết lập mức kỹ sư"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);

    m_adminTab = new AdminTab(m_tabs);
    m_adTab = new AdTab(m_tabs);
    m_swTab = new SwTab(m_tabs);
    m_otherTab = new OtherTab(m_tabs);
    m_paramsTab = new ParamsTab(m_tabs);
    m_commandTabs = {m_adminTab, m_adTab, m_swTab, m_otherTab, m_paramsTab};

    m_tabs->addTab(m_adminTab, QStringLiteral("ADMIN"));
    m_tabs->addTab(m_adTab, QStringLiteral("AD"));
    m_tabs->addTab(m_swTab, QStringLiteral("SW"));
    m_tabs->addTab(m_otherTab, QStringLiteral("Other"));
    m_tabs->addTab(m_paramsTab, QStringLiteral("Params"));
    m_tabs->addTab(buildConnectTab(), QStringLiteral("Connect"));
    m_tabs->setCurrentIndex(m_tabs->count() - 1);

    // Serial của gói phản hồi "sát bên phải tab": một nhãn chung ở góc thanh
    // tab, đổi nội dung theo tab đang xem.
    m_cornerSerial = new QLabel(m_tabs);
    m_cornerSerial->setStyleSheet(QStringLiteral("color:#8a95a1;padding-right:6px;"));
    m_tabs->setCornerWidget(m_cornerSerial, Qt::TopRightCorner);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { updateCornerSerial(); });
    for (EngineerTab *t : std::as_const(m_commandTabs))
        connect(t, &EngineerTab::cornerSerialChanged, this, &EngineerWindow::updateCornerSerial);

    m_blocks = {m_adminTab->block(), m_adTab->block(), m_swTab->block(),
                m_otherTab->other(), m_otherTab->calibReg(), m_otherTab->buphabd()};
    for (CommandBlock *b : std::as_const(m_blocks))
        wireBlock(b);

    // Đổi chế độ hiệu chuẩn bên tab ADMIN thì kéo theo cặp tần số bên tab AD.
    connect(m_adminTab, &AdminTab::calibPresetRequested, m_adTab, &AdTab::selectPreset);

    connect(m_otherTab, &OtherTab::rebootRequested, this, &EngineerWindow::rebootRequested);

    lay->addWidget(m_tabs, 1);
    updateCornerSerial();

    // Ô khoá ở góc dưới bên trái theo đặc tả.
    auto *bottom = new QHBoxLayout;
    m_lockBox = new QCheckBox(QStringLiteral("Khóa điều khiển"), this);
    m_lockBox->setChecked(Settings::instance().adminLocked());
    bottom->addWidget(m_lockBox);
    bottom->addStretch(1);
    auto *closeBtn = new QPushButton(QStringLiteral("Đóng"), this);
    bottom->addWidget(closeBtn);
    lay->addLayout(bottom);

    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(m_lockBox, &QCheckBox::toggled, this, [this](bool locked) {
        Settings::instance().setAdminLocked(locked);
        for (EngineerTab *t : std::as_const(m_commandTabs))
            t->setLocked(locked);
    });
    for (EngineerTab *t : std::as_const(m_commandTabs))
        t->setLocked(m_lockBox->isChecked());

    // Kích thước tự nhiên: chiều ngang vừa đúng bảng 8 cột của tab "Connect"
    // (hẹp hơn là kỹ sư phải cuộn ngang mới thấy RemoteIP/RemotePort), chiều
    // cao lấy đủ cho tab "ADMIN" — tab dài nhất — nhưng không quá màn hình.
    int tableWidth = 2 * m_links->frameWidth()
                     + m_links->verticalScrollBar()->sizeHint().width();
    for (int c = 0; c < m_links->columnCount(); ++c)
        tableWidth += m_links->columnWidth(c);

    layout()->activate();
    const QRect avail = screen() ? screen()->availableGeometry() : QRect(0, 0, 1280, 1024);
    const QSize natural(qBound(560, tableWidth + 44, 1100),
                        qMin(qMax(sizeHint().height(), 940), int(avail.height() * 0.92)));
    resize(natural);

    // Lấy xong kích thước tự nhiên thì hạ sàn của hai bảng xuống còn 3 dòng và
    // đặt sàn của cửa sổ đúng một nửa: anh Linh muốn kéo thu nhỏ được tối đa
    // 50% cả hai chiều, lúc đó hai bảng tự hiện thanh cuộn.
    relaxTable(m_links);
    relaxTable(m_nodes);
    setMinimumSize(natural.width() / 2, natural.height() / 2);
}

// Sàn chiều cao sau khi đã lấy xong kích thước tự nhiên: chỉ còn một dòng, đủ
// để cửa sổ co lại bằng nửa mà các nút bên dưới bảng không bị bảng đè lên.
void EngineerWindow::relaxTable(QTableWidget *table)
{
    const int rowHeight = table->verticalHeader()->defaultSectionSize();
    const int headerHeight = table->horizontalHeader()->sizeHint().height();
    table->setMinimumHeight(headerHeight + rowHeight + 2 * table->frameWidth());
}

// Ép dòng cao cố định rồi tính ngược chiều cao bảng: lúc dựng giao diện bảng
// chưa hiện nên chiều cao dòng tự co chưa có giá trị thật.
void EngineerWindow::fitTable(QTableWidget *table, int visibleRows)
{
    const int rowHeight = qMax(26, fontMetrics().height() + 10);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);

    const int headerHeight = table->horizontalHeader()->sizeHint().height();
    const int scrollBar = table->horizontalScrollBar()->sizeHint().height();
    table->setMinimumHeight(headerHeight + rowHeight * visibleRows
                            + 2 * table->frameWidth() + scrollBar);
}

// -------------------------------------------------------- lệnh mức kỹ sư

void EngineerWindow::wireBlock(CommandBlock *block)
{
    connect(block, &CommandBlock::sendRequested, this, [this, block] {
        // Đổi chế độ hiệu chuẩn thì AD9361 phải đổi tần số trước, nếu không hệ
        // thống hiệu chuẩn ở sai tần số: gửi CMD_ADMIN_AD rồi 100 ms sau mới
        // gửi CMD_ADMIN.
        if (block == m_adminTab->block() && m_adminTab->needsAdFirst()) {
            emit commandReady(m_adTab->block()->category(), m_adTab->block()->fields());
            QTimer::singleShot(100, this, [this, block] {
                emit commandReady(block->category(), block->fields());
            });
            return;
        }
        emit commandReady(block->category(), block->fields());
    });
}

void EngineerWindow::applyFrame(quint32 category, quint32 serial, const quint32 *fields, int count)
{
    if (category == Proto::CatStatusCalib) {
        if (count >= StatusCalib::Count)
            m_adminTab->applyCalibStatus(fields, serial);
        return;
    }
    if (category == Proto::CatStatusParams) {
        if (count >= StatusParams::kCount)
            m_paramsTab->applyParams(fields, serial);
        return;
    }
    for (CommandBlock *b : std::as_const(m_blocks)) {
        if (b->backCategory() == category && count >= b->fields().size()) {
            b->applyBack(fields, serial);
            return;
        }
    }
}

void EngineerWindow::noteSent(quint32 category, quint32 serial)
{
    for (CommandBlock *b : std::as_const(m_blocks)) {
        if (b->category() == category) {
            b->noteSent(serial);
            return;
        }
    }
}

void EngineerWindow::clearBack()
{
    for (EngineerTab *t : std::as_const(m_commandTabs))
        t->clearBack();
    updateCornerSerial();
}

void EngineerWindow::setLocked(bool locked)
{
    m_lockBox->setChecked(locked);
}

void EngineerWindow::updateCornerSerial()
{
    QWidget *current = m_tabs->currentWidget();
    auto *tab = qobject_cast<EngineerTab *>(current);
    m_cornerSerial->setText(tab ? tab->cornerSerialText() : QString());
}

// ------------------------------------------------------------- tab Connect

QWidget *EngineerWindow::buildConnectTab()
{
    auto *page = new QWidget(m_tabs);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    // --- Group: thiết lập gửi/nhận dữ liệu
    auto *linkBox = new QGroupBox(QStringLiteral("Thiết lập gửi/nhận dữ liệu"), page);
    auto *linkLay = new QVBoxLayout(linkBox);
    linkLay->setContentsMargins(8, 6, 8, 8);

    m_links = new QTableWidget(0, 8, linkBox);
    m_links->setHorizontalHeaderLabels({QStringLiteral("Phân loại"), QStringLiteral("Send/Recv"),
                                        QStringLiteral("TCP/UDP"), QStringLiteral("Type"),
                                        QStringLiteral("LocalIP"), QStringLiteral("LocalPort"),
                                        QStringLiteral("RemoteIP"), QStringLiteral("RemotePort")});
    m_links->verticalHeader()->setVisible(false);
    m_links->setSelectionMode(QAbstractItemView::NoSelection);
    m_links->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    linkLay->addWidget(m_links);

    auto *linkBtns = new QHBoxLayout;
    auto *saveLinkBtn = new QPushButton(QStringLiteral("Lưu cấu hình"), linkBox);
    // Không có thêm/xoá dòng: đủ 10 loại dữ liệu, cần khác thì kỹ sư sửa file.
    auto *hint = new QLabel(QStringLiteral("Đổi xong phải khởi động lại phần mềm."), linkBox);
    hint->setStyleSheet(QStringLiteral("color:#8a95a1;font-style:italic;"));
    linkBtns->addWidget(hint);
    linkBtns->addStretch(1);
    linkBtns->addWidget(saveLinkBtn);
    linkLay->addLayout(linkBtns);
    // Hệ số giãn = số dòng muốn thấy trừ đi sàn một dòng của relaxTable(), nhờ
    // vậy ở kích thước tự nhiên phần dư chia ra đúng 10 dòng và 8 dòng.
    lay->addWidget(linkBox, 9);

    // --- Group: danh sách các nút mạng
    auto *nodeBox = new QGroupBox(QStringLiteral("Danh sách các nút mạng"), page);
    auto *nodeLay = new QVBoxLayout(nodeBox);
    nodeLay->setContentsMargins(8, 6, 8, 8);

    m_nodes = new QTableWidget(0, 3, nodeBox);
    m_nodes->setHorizontalHeaderLabels({QStringLiteral("Tên"), QStringLiteral("Địa chỉ IP"),
                                        QStringLiteral("Phân loại")});
    m_nodes->verticalHeader()->setVisible(false);
    m_nodes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nodes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_nodes->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_nodes->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    nodeLay->addWidget(m_nodes);

    auto *nodeBtns = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("Thêm dòng"), nodeBox);
    auto *delBtn = new QPushButton(QStringLiteral("Xóa dòng"), nodeBox);
    auto *saveNodeBtn = new QPushButton(QStringLiteral("Lưu cấu hình"), nodeBox);
    nodeBtns->addWidget(addBtn);
    nodeBtns->addWidget(delBtn);
    nodeBtns->addStretch(1);
    nodeBtns->addWidget(saveNodeBtn);
    nodeLay->addLayout(nodeBtns);
    lay->addWidget(nodeBox, 7);

    fillLinkTable();
    fillNodeTable();
    // Đủ 10 dòng cấu hình cổng (không cho thêm/xoá) và 8 dòng nút mạng.
    fitTable(m_links, 10);
    fitTable(m_nodes, 8);

    connect(saveLinkBtn, &QPushButton::clicked, this, &EngineerWindow::saveLinks);
    connect(saveNodeBtn, &QPushButton::clicked, this, &EngineerWindow::saveNodes);
    connect(addBtn, &QPushButton::clicked, this, [this] {
        const int row = m_nodes->rowCount();
        m_nodes->insertRow(row);
        setNodeRow(row, NetNode{QStringLiteral("Nút mới"), QStringLiteral("127.0.0.1"), 0});
        m_nodes->selectRow(row);
    });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        const int row = m_nodes->currentRow();
        if (row >= 0)
            m_nodes->removeRow(row);
    });

    return page;
}

QWidget *EngineerWindow::makeIpEdit(const QString &value)
{
    auto *e = new QLineEdit(value);
    e->setValidator(ipValidator(e));
    e->setMinimumWidth(110);
    return e;
}

QWidget *EngineerWindow::makePortSpin(quint16 value)
{
    auto *s = new QSpinBox;
    // Cổng 0 có nghĩa riêng: gửi từ cổng bất kỳ, hoặc nhận từ cổng remote bất kỳ.
    s->setRange(0, 65535);
    s->setValue(int(value));
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    s->setMinimumWidth(62);
    return s;
}

void EngineerWindow::fillLinkTable()
{
    const LinkConfig cfg = LinkConfig::load();
    const QStringList categories = LinkConfig::categoryNames();

    m_links->setRowCount(cfg.entries.size());
    for (int r = 0; r < cfg.entries.size(); ++r) {
        const LinkEntry &e = cfg.entries.at(r);

        QComboBox *cat = makeCombo(categories, int(categories.indexOf(e.category)));
        if (!categories.contains(e.category)) {
            cat->addItem(e.category);
            cat->setCurrentText(e.category);
        }
        m_links->setCellWidget(r, 0, cat);
        m_links->setCellWidget(r, 1, makeCombo(kDirNames, e.direction));

        QComboBox *proto = makeCombo(kProtoNames, e.protocol);
        QComboBox *type = makeCombo(e.protocol == LinkEntry::Tcp ? kTcpTypes : kUdpTypes, e.type);
        m_links->setCellWidget(r, 2, proto);
        m_links->setCellWidget(r, 3, type);

        // Danh sách của cột Type phụ thuộc cột TCP/UDP ngay bên trái.
        connect(proto, QOverload<int>::of(&QComboBox::currentIndexChanged), type,
                [type](int index) {
                    const int keep = type->currentIndex();
                    type->clear();
                    type->addItems(index == LinkEntry::Tcp ? kTcpTypes : kUdpTypes);
                    type->setCurrentIndex(qBound(0, keep, 1));
                });

        m_links->setCellWidget(r, 4, makeIpEdit(e.localIp));
        m_links->setCellWidget(r, 5, makePortSpin(e.localPort));
        m_links->setCellWidget(r, 6, makeIpEdit(e.remoteIp));
        m_links->setCellWidget(r, 7, makePortSpin(e.remotePort));
    }
}

void EngineerWindow::saveLinks()
{
    LinkConfig cfg = LinkConfig::load();
    QVector<LinkEntry> entries;

    for (int r = 0; r < m_links->rowCount(); ++r) {
        LinkEntry e;
        e.category = comboText(m_links, r, 0);
        e.direction = comboIndex(m_links, r, 1);
        e.protocol = comboIndex(m_links, r, 2);
        e.type = comboIndex(m_links, r, 3);
        e.localIp = editText(m_links, r, 4);
        e.localPort = spinValue(m_links, r, 5);
        e.remoteIp = editText(m_links, r, 6);
        e.remotePort = spinValue(m_links, r, 7);

        if (e.localIp.isEmpty() || e.remoteIp.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Địa chỉ IP"),
                                 QStringLiteral("Dòng %1 (%2) có địa chỉ IP chưa đủ bốn nhóm số.")
                                     .arg(r + 1).arg(e.category));
            return;
        }
        entries.append(e);
    }

    cfg.entries = entries;
    if (!cfg.save()) {
        QMessageBox::warning(this, QStringLiteral("Lưu cấu hình"),
                             QStringLiteral("Không ghi được settings/connect.json."));
        return;
    }
    emit configSaved(QStringLiteral("Đã lưu settings/connect.json — khởi động lại phần mềm "
                                    "để áp dụng thay đổi."));
}

void EngineerWindow::setNodeRow(int row, const NetNode &node)
{
    m_nodes->setItem(row, 0, new QTableWidgetItem(node.name));
    m_nodes->setCellWidget(row, 1, makeIpEdit(node.address));
    m_nodes->setCellWidget(row, 2, makeCombo(kNodeKinds, node.kind));
}

void EngineerWindow::fillNodeTable()
{
    const QVector<NetNode> &nodes = Settings::instance().netNodes();
    m_nodes->setRowCount(nodes.size());
    for (int r = 0; r < nodes.size(); ++r)
        setNodeRow(r, nodes.at(r));
}

void EngineerWindow::saveNodes()
{
    QVector<NetNode> nodes;
    for (int r = 0; r < m_nodes->rowCount(); ++r) {
        NetNode n;
        const QTableWidgetItem *nameItem = m_nodes->item(r, 0);
        n.name = nameItem ? nameItem->text().trimmed() : QString();
        n.address = editText(m_nodes, r, 1);
        n.kind = comboIndex(m_nodes, r, 2);

        if (n.address.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Địa chỉ IP"),
                                 QStringLiteral("Dòng %1 có địa chỉ IP chưa đủ bốn nhóm số.")
                                     .arg(r + 1));
            return;
        }
        if (n.name.isEmpty())
            n.name = n.address;
        nodes.append(n);
    }

    Settings::instance().setNetNodes(nodes);
    emit configSaved(QStringLiteral("Đã lưu settings/checkip.json — khởi động lại phần mềm "
                                    "để áp dụng thay đổi."));
}
