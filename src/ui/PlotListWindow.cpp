#include "ui/PlotListWindow.h"

#include "proto/Packets.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QScrollBar>
#include <QTableView>
#include <QVBoxLayout>

// ------------------------------------------------------------------ model

PlotListModel::PlotListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void PlotListModel::append(const quint32 *f)
{
    if (int(m_rows.size()) >= kMaxRows) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_rows.pop_front();
        endRemoveRows();
    }
    const int row = int(m_rows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_rows.push_back(Row{m_nextStt++, QTime::currentTime(), f[Plot::Azm], f[Plot::Range], f[Plot::Retmode],
                         f[Plot::Commander], f[Plot::Flightid], f[Plot::Altitude], f[Plot::Fuellevel]});
    endInsertRows();
}

void PlotListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_nextStt = 1;
    endResetModel();
}

int PlotListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int PlotListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant PlotListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return QVariant();
    if (role == Qt::TextAlignmentRole)
        return int((index.column() == ColTime ? Qt::AlignHCenter : Qt::AlignRight) | Qt::AlignVCenter);
    if (role != Qt::DisplayRole)
        return QVariant();

    const Row &r = m_rows.at(size_t(index.row()));
    switch (index.column()) {
    case ColStt:       return QString::number(r.stt);
    case ColTime:      return r.time.toString(QStringLiteral("HH:mm:ss.zzz"));
    case ColAzm:       return QString::number((r.azm % 36000u) / 100.0, 'f', 2);
    // Các cột còn lại hiện đúng giá trị nhận được (step-06 mục 11).
    case ColRange:     return QString::number(r.range);
    case ColMode:      return QString::number(r.retmode);
    case ColCommander: return QString::number(r.commander);
    case ColFlightid:  return QString::number(r.flightid);
    case ColAltitude:  return QString::number(r.altitude);
    case ColFuel:      return QString::number(r.fuel);
    default:           return QVariant();
    }
}

QVariant PlotListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case ColStt:       return QStringLiteral("STT");
    case ColTime:      return QStringLiteral("Thời gian");
    case ColAzm:       return QStringLiteral("P.Vị");
    case ColRange:     return QStringLiteral("C.Ly");
    case ColMode:      return QStringLiteral("Chế độ");
    case ColCommander: return QStringLiteral("Chỉ huy");
    case ColFlightid:  return QStringLiteral("S.Hiệu");
    case ColAltitude:  return QStringLiteral("Độ cao");
    case ColFuel:      return QStringLiteral("N.Liệu");
    default:           return QVariant();
    }
}

QString PlotListModel::widestText(int column)
{
    switch (column) {
    case ColStt:       return QStringLiteral("0000000");
    case ColTime:      return QStringLiteral("00:00:00.000");
    case ColAzm:       return QStringLiteral("359.99");
    case ColRange:     return QStringLiteral("0000000");
    case ColMode:      return QStringLiteral("Chế độ");
    case ColCommander: return QStringLiteral("Chỉ huy");
    case ColFlightid:  return QStringLiteral("0000000");
    case ColAltitude:  return QStringLiteral("000000");
    case ColFuel:      return QStringLiteral("N.Liệu");
    default:           return QString();
    }
}

// ----------------------------------------------------------------- window

PlotListWindow::PlotListWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                          | Qt::WindowMaximizeButtonHint | Qt::WindowStaysOnTopHint)
{
    setObjectName(QStringLiteral("PlotListWindow"));
    setWindowTitle(QStringLiteral("Điểm dấu MH"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    m_model = new PlotListModel(this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->verticalHeader()->setVisible(false);
    m_view->verticalHeader()->setDefaultSectionSize(qMax(20, fontMetrics().height() + 4));
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    QHeaderView *h = m_view->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    h->setStretchLastSection(true);
    h->setHighlightSections(false);
    const QFontMetrics fm = m_view->fontMetrics();
    int total = 0;
    for (int c = 0; c < PlotListModel::ColCount; ++c) {
        const int w = fm.horizontalAdvance(PlotListModel::widestText(c)) + 18;
        m_view->setColumnWidth(c, w);
        total += w;
    }
    lay->addWidget(m_view, 1);

    auto *row = new QHBoxLayout;
    auto *clearBtn = new QPushButton(QStringLiteral("Xóa danh sách"), this);
    clearBtn->setToolTip(QStringLiteral("Xóa toàn bộ bảng danh sách điểm dấu MH"));
    row->addWidget(clearBtn);
    row->addStretch(1);
    lay->addLayout(row);

    connect(clearBtn, &QPushButton::clicked, m_model, &PlotListModel::clear);

    // Vừa đủ chín cột không phải cuộn ngang, cao chừng hai chục dòng.
    resize(total + 40, 520);
    setMinimumSize(320, 200);
}

void PlotListWindow::addPlot(const quint32 *plotFields)
{
    // Chỉ bám dòng mới nhất khi trắc thủ đang ở cuối bảng: kéo lên đọc dòng
    // cũ thì không bị giật xuống mỗi lần có điểm dấu.
    const QScrollBar *bar = m_view->verticalScrollBar();
    const bool follow = bar->value() >= bar->maximum();
    m_model->append(plotFields);
    if (follow && isVisible())
        m_view->scrollToBottom();
}

void PlotListWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_view->scrollToBottom();
}
