#include "ui/Popups.h"

#include "ui/Theme.h"

#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTime>
#include <QVBoxLayout>

namespace {

QPixmap statusDot(const QColor &c)
{
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c.darker(160), 1));
    p.setBrush(c);
    p.drawEllipse(QRectF(1.5, 1.5, 9, 9));
    return pm;
}

QString escape(const QString &s)
{
    return s.toHtmlEscaped();
}

} // namespace

// ------------------------------------------------------------- Thông báo

NotifyPopup::NotifyPopup(QWidget *parent)
    : SlidePopup(QStringLiteral("Thông báo hệ thống"), SlidePopup::FromLeft, parent)
{
    auto *lay = new QVBoxLayout(body());
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(5);

    m_log = new QTextEdit(body());
    m_log->setReadOnly(true);
    m_log->setMinimumSize(430, 220);
    m_log->setLineWrapMode(QTextEdit::WidgetWidth);
    lay->addWidget(m_log, 1);

    m_clearBtn = new QPushButton(QStringLiteral("Xóa thông báo"), body());
    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_clearBtn);
    lay->addLayout(row);

    connect(m_clearBtn, &QPushButton::clicked, this, [this] {
        m_log->clear();
        emit cleared();
    });
}

void NotifyPopup::append(const QString &message, bool isError)
{
    const QString stamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    QString html = QStringLiteral("<span style='color:#4da6ff;'>[%1]</span> ").arg(stamp);
    if (isError)
        html += QStringLiteral("<span style='color:#ff4d4d;'>[Lỗi]</span> ");
    html += QStringLiteral("<span style='color:#d3dbe3;'>%1</span>").arg(escape(message));

    m_log->append(html);
    // Luôn kéo xuống thông báo mới nhất.
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

// -------------------------------------------------------- Kết nối mạng

NetworkPopup::NetworkPopup(QWidget *parent)
    : SlidePopup(QStringLiteral("Trạng thái kết nối mạng"), SlidePopup::FromLeft, parent)
{
    auto *lay = new QVBoxLayout(body());
    lay->setContentsMargins(6, 6, 6, 6);

    m_table = new QTableWidget(0, 3, body());
    m_table->setHorizontalHeaderLabels({QStringLiteral("Tên"), QStringLiteral("Địa chỉ"),
                                        QStringLiteral("Trạng thái")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setMinimumWidth(340);
    lay->addWidget(m_table);
}

// Chiều cao đủ đúng 10 dòng; từ dòng thứ 11 mới hiện thanh cuộn dọc.
void NetworkPopup::fitRows(int visibleRows)
{
    // Ép mọi dòng cùng một chiều cao rồi tính ngược ra chiều cao bảng. Nếu để
    // Qt tự co dòng thì lúc dựng giao diện (bảng chưa hiện) chiều cao dòng chưa
    // có giá trị thật, và bảng lệch đi vài dòng.
    const int rowHeight = qMax(24, fontMetrics().height() + 8);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);

    const int headerHeight = m_table->horizontalHeader()->sizeHint().height();
    m_table->setFixedHeight(headerHeight + rowHeight * visibleRows + 2 * m_table->frameWidth());
}

void NetworkPopup::setNodes(const QVector<NetNode> &nodes)
{
    m_table->setRowCount(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(nodes.at(i).name));
        m_table->setItem(i, 1, new QTableWidgetItem(nodes.at(i).address));
        // Đặt hình tròn bằng QLabel thay vì icon của item: icon trong bảng luôn
        // bám lề trái, không căn giữa cột được.
        auto *dot = new QLabel(m_table);
        dot->setAlignment(Qt::AlignCenter);
        dot->setPixmap(statusDot(Theme::kError));
        m_table->setCellWidget(i, 2, dot);
    }
    fitRows(10);
}

void NetworkPopup::setNodeState(int index, bool alive)
{
    if (index < 0 || index >= m_table->rowCount())
        return;
    if (auto *dot = qobject_cast<QLabel *>(m_table->cellWidget(index, 2)))
        dot->setPixmap(statusDot(alive ? Theme::kOk : Theme::kError));
}

// --------------------------------------------------------- Tọa độ tâm đài

RadarCenterPopup::RadarCenterPopup(QWidget *parent)
    : SlidePopup(QStringLiteral("Tọa độ tâm đài"), SlidePopup::FromRight, parent)
{
    auto *lay = new QVBoxLayout(body());
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto *validator = new QDoubleValidator(-180.0, 180.0, 6, this);
    validator->setNotation(QDoubleValidator::StandardNotation);

    // Lưới hai cột: nhãn và ô nhập mỗi bên một cột nên hai ô nhập luôn rộng
    // bằng nhau dù nhãn dài ngắn khác nhau.
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);

    auto addRow = [&](int row, const QString &caption, QLineEdit **edit) {
        auto *lbl = new QLabel(caption, body());
        *edit = new QLineEdit(body());
        (*edit)->setValidator(validator);
        (*edit)->setMinimumWidth(130);
        grid->addWidget(lbl, row, 0);
        grid->addWidget(*edit, row, 1);
    };
    addRow(0, QStringLiteral("Vỹ độ:"), &m_lat);
    addRow(1, QStringLiteral("Kinh độ:"), &m_lon);
    lay->addLayout(grid);

    auto *applyBtn = new QPushButton(QStringLiteral("Áp dụng"), body());
    auto *gpsBtn = new QPushButton(QStringLiteral("Đặt theo GPS"), body());
    auto *centerBtn = new QPushButton(QStringLiteral("Căn chỉnh về giữa"), body());
    lay->addWidget(applyBtn);
    lay->addWidget(gpsBtn);
    lay->addWidget(centerBtn);

    connect(applyBtn, &QPushButton::clicked, this, [this] {
        bool okLat = false, okLon = false;
        const double lat = m_lat->text().trimmed().toDouble(&okLat);
        const double lon = m_lon->text().trimmed().toDouble(&okLon);
        if (okLat && okLon)
            emit applyRequested(lat, lon);
    });
    connect(gpsBtn, &QPushButton::clicked, this, &RadarCenterPopup::gpsRequested);
    connect(centerBtn, &QPushButton::clicked, this, &RadarCenterPopup::recenterRequested);
}

void RadarCenterPopup::setCenter(double lat, double lon)
{
    m_lat->setText(QString::number(lat, 'f', 6));
    m_lon->setText(QString::number(lon, 'f', 6));
}

// ------------------------------------------------------------- khung sẵn

PlaceholderPopup::PlaceholderPopup(const QString &title, const QString &note, QWidget *parent)
    : SlidePopup(title, SlidePopup::FromLeft, parent)
{
    auto *lay = new QVBoxLayout(body());
    lay->setContentsMargins(14, 14, 14, 14);
    auto *lbl = new QLabel(note, body());
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QStringLiteral("color:#5d666f;font-style:italic;"));
    lbl->setMinimumSize(260, 90);
    lay->addWidget(lbl);
}
