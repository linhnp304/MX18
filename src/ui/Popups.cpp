#include "ui/Popups.h"

#include "ui/Theme.h"

#include <QDoubleValidator>
#include <QHBoxLayout>
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
    m_table->setMinimumSize(340, 170);
    lay->addWidget(m_table);
}

void NetworkPopup::setNodes(const QVector<NetNode> &nodes)
{
    m_table->setRowCount(nodes.size());
    for (int i = 0; i < nodes.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(nodes.at(i).name));
        m_table->setItem(i, 1, new QTableWidgetItem(nodes.at(i).address));
        auto *st = new QTableWidgetItem();
        st->setIcon(QIcon(statusDot(Theme::kError)));
        st->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 2, st);
    }
    m_table->resizeRowsToContents();
}

void NetworkPopup::setNodeState(int index, bool alive)
{
    if (index < 0 || index >= m_table->rowCount())
        return;
    if (QTableWidgetItem *it = m_table->item(index, 2))
        it->setIcon(QIcon(statusDot(alive ? Theme::kOk : Theme::kError)));
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

    auto addRow = [&](const QString &caption, QLineEdit **edit) {
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel(caption, body());
        lbl->setMinimumWidth(70);
        *edit = new QLineEdit(body());
        (*edit)->setValidator(validator);
        (*edit)->setMinimumWidth(130);
        row->addWidget(lbl);
        row->addWidget(*edit, 1);
        lay->addLayout(row);
    };
    addRow(QStringLiteral("Vỹ độ (lat):"), &m_lat);
    addRow(QStringLiteral("Kinh độ (lng):"), &m_lon);

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
