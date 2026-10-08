#include "ui/TrackListTab.h"

#include "proto/Packets.h"
#include "track/TrackStore.h"

#include <QGridLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

// ------------------------------------------------------------------ model

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void TrackListModel::setStore(TrackStore *store)
{
    beginResetModel();
    if (m_store)
        m_store->disconnect(this);
    m_store = store;
    m_ids.clear();
    if (m_store) {
        for (const TrackEntry &t : m_store->tracks())
            m_ids.append(t.id());
        connect(m_store, &TrackStore::trackUpdated, this, &TrackListModel::onUpdated);
        connect(m_store, &TrackStore::trackChanged, this, &TrackListModel::onChanged);
        connect(m_store, &TrackStore::trackRemoved, this,
                [this](const TrackEntry &t, int) { onRemoved(t); });
    }
    endResetModel();
}

const TrackEntry *TrackListModel::entryAt(int row) const
{
    if (!m_store || row < 0 || row >= m_ids.size())
        return nullptr;
    // Thứ tự dòng trùng thứ tự trong TrackStore (cùng thêm cuối, cùng rút giữa
    // chừng) nên thường lấy thẳng theo chỉ số; chỉ dò lại khi hai bên lệch nhau.
    const quint32 id = m_ids.at(row);
    const QVector<TrackEntry> &all = m_store->tracks();
    if (row < all.size() && all.at(row).id() == id)
        return &all.at(row);
    return m_store->find(id);
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_ids.size());
}

int TrackListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QString TrackListModel::widestText(int column)
{
    switch (column) {
    case ColFollow:   return QStringLiteral("Theo dõi");
    case ColTop:      return QStringLiteral("40950");
    case ColPosition: return QStringLiteral("359.999° - 359.999km");
    case ColSpeed:    return QStringLiteral("1199.999km/h");
    case ColHeading:  return QStringLiteral("359.999°");
    default:          return QString();
    }
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    const TrackEntry *t = entryAt(index.row());
    if (!t)
        return QVariant();
    const int col = index.column();

    if (role == Qt::CheckStateRole && col == ColFollow)
        return t->followed ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::TextAlignmentRole && col != ColFollow)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (col) {
    case ColTop:
        return QString::number(t->f[Track::TrackTop]);
    case ColPosition:
        return QStringLiteral("%1° - %2km")
            .arg(t->azimuthDeg(), 0, 'f', 3)
            .arg(t->rangeM() / 1000.0, 0, 'f', 3);
    case ColSpeed:
        return QStringLiteral("%1km/h").arg(t->f[Track::Velocity] * 3.6, 0, 'f', 3);
    case ColHeading:
        return QStringLiteral("%1°").arg(t->f[Track::Heading] / 100.0, 0, 'f', 3);
    default:
        return QVariant();
    }
}

QVariant TrackListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case ColFollow:   return QStringLiteral("Theo dõi");
    case ColTop:      return QStringLiteral("Tốp");
    case ColPosition: return QStringLiteral("Vị trí");
    case ColSpeed:    return QStringLiteral("Vận tốc");
    case ColHeading:  return QStringLiteral("Hướng");
    default:          return QVariant();
    }
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    // Chỉ cột "Theo dõi" đổi được; các cột khác chỉ đọc.
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColFollow)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

bool TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!m_store || role != Qt::CheckStateRole || index.column() != ColFollow
        || index.row() >= m_ids.size())
        return false;
    // Đi vòng qua TrackStore để menu chuột phải trên panel 1 và ô này luôn khớp
    // nhau; dòng được vẽ lại khi TrackStore báo trackChanged.
    m_store->setFollowed(m_ids.at(index.row()), value.toInt() == Qt::Checked);
    return true;
}

void TrackListModel::onUpdated(const TrackEntry &t, bool added)
{
    if (added) {
        const int row = int(m_ids.size());
        beginInsertRows(QModelIndex(), row, row);
        m_ids.append(t.id());
        endInsertRows();
        return;
    }
    onChanged(t.id());
}

void TrackListModel::onChanged(quint32 id)
{
    const int row = int(m_ids.indexOf(id));
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void TrackListModel::onRemoved(const TrackEntry &t)
{
    const int row = int(m_ids.indexOf(t.id()));
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_ids.remove(row);
    endRemoveRows();
}

// -------------------------------------------------------------------- tab

TrackListTab::TrackListTab(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    m_model = new TrackListModel(this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->verticalHeader()->setVisible(false);
    m_view->verticalHeader()->setDefaultSectionSize(qMax(20, fontMetrics().height() + 4));
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Bề rộng cột đặt một lần theo chuỗi dài nhất: ResizeToContents đo lại cả
    // bảng mỗi lần một quỹ đạo cập nhật. Panel hẹp thì cuộn ngang.
    QHeaderView *h = m_view->horizontalHeader();
    h->setSectionResizeMode(QHeaderView::Interactive);
    h->setStretchLastSection(true);
    h->setHighlightSections(false);
    const QFontMetrics fm = m_view->fontMetrics();
    for (int c = 0; c < TrackListModel::ColCount; ++c)
        m_view->setColumnWidth(c, fm.horizontalAdvance(TrackListModel::widestText(c)) + 16);
    lay->addWidget(m_view, 1);

    auto *plotListBtn = new QPushButton(QStringLiteral("Danh sách điểm dấu MH"), this);
    plotListBtn->setToolTip(QStringLiteral("Mở cửa sổ danh sách điểm dấu MH"));
    auto *clearPlotsBtn = new QPushButton(QStringLiteral("Xóa điểm dấu MH"), this);
    clearPlotsBtn->setToolTip(QStringLiteral("Xóa toàn bộ điểm dấu MH đang hiển thị trên bản đồ"));
    auto *removeBtn = new QPushButton(QStringLiteral("Xóa quỹ đạo"), this);
    removeBtn->setToolTip(QStringLiteral("Xóa quỹ đạo của dòng đang chọn"));
    auto *clearBtn = new QPushButton(QStringLiteral("Xóa danh sách quỹ đạo"), this);
    clearBtn->setToolTip(QStringLiteral("Xóa toàn bộ danh sách quỹ đạo và vết"));

    // "Xóa quỹ đạo" dưới cùng bên trái (step-06 mục 11), hàng nút điểm dấu MH
    // nằm ngay trên. Hai cột chia đều để nút không đổi cỡ theo chữ.
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    grid->addWidget(plotListBtn, 0, 0);
    grid->addWidget(clearPlotsBtn, 0, 1);
    grid->addWidget(removeBtn, 1, 0);
    grid->addWidget(clearBtn, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    // Panel 2 chỉ ~320 px ở màn 1280: lề ngang hẹp để hai nút chữ dài nhất vẫn
    // vừa một hàng mà không cắt chữ.
    for (QPushButton *b : {plotListBtn, clearPlotsBtn, removeBtn, clearBtn}) {
        b->setStyleSheet(QStringLiteral("QPushButton { padding: 4px 4px; }"));
        // minimumSizeHint của nút nhỏ hơn chữ: không ghim thì lưới chia đều cắt
        // mất chữ đầu thay vì nới panel ra vài điểm ảnh.
        b->setMinimumWidth(b->sizeHint().width());
    }
    lay->addLayout(grid);

    connect(m_view, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        // Bấm đúp trúng ô "Theo dõi" là bấm vào ô đánh dấu, không mở cửa sổ.
        if (index.isValid() && index.column() != TrackListModel::ColFollow)
            emit trackActivated(m_model->idAt(index.row()));
    });
    connect(plotListBtn, &QPushButton::clicked, this, &TrackListTab::plotListRequested);
    connect(clearPlotsBtn, &QPushButton::clicked, this, &TrackListTab::clearPlotsRequested);
    connect(removeBtn, &QPushButton::clicked, this, &TrackListTab::removeSelected);
    connect(clearBtn, &QPushButton::clicked, this, &TrackListTab::clearAll);
}

void TrackListTab::setTrackStore(TrackStore *store)
{
    m_store = store;
    m_model->setStore(store);
}

void TrackListTab::removeSelected()
{
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
    if (!m_store || rows.isEmpty()) {
        emit notice(QStringLiteral("Chọn một dòng trong danh sách quỹ đạo rồi mới bấm \"Xóa quỹ đạo\"."));
        return;
    }
    m_store->remove(m_model->idAt(rows.first().row()), TrackStore::RemovedByUser);
}

void TrackListTab::clearAll()
{
    if (!m_store)
        return;
    const int count = int(m_store->tracks().size());
    m_store->removeAll(TrackStore::RemovedListCleared);
    emit notice(QStringLiteral("Đã xóa danh sách quỹ đạo (%1 quỹ đạo).").arg(count));
}
