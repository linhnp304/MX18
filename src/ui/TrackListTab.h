#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QWidget>

class QPushButton;
class QTableView;
class TrackStore;
struct TrackEntry;

// Bảng quỹ đạo đọc thẳng từ TrackStore: mỗi dòng là một track_id, đúng thứ tự
// xuất hiện. Model giữ danh sách id riêng vì TrackStore báo trackRemoved sau
// khi đã rút quỹ đạo ra — lúc đó chỉ còn id để tìm dòng cần bỏ.
class TrackListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColFollow = 0, ColTop, ColPosition, ColSpeed, ColHeading, ColCount };

    explicit TrackListModel(QObject *parent = nullptr);

    void setStore(TrackStore *store);
    quint32 idAt(int row) const { return m_ids.value(row); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // Chuỗi dài nhất mỗi cột, để đặt bề rộng cột một lần thay vì đo lại nội
    // dung mỗi lần X18-VQ cập nhật.
    static QString widestText(int column);

private:
    const TrackEntry *entryAt(int row) const;
    void onUpdated(const TrackEntry &t, bool added);
    void onChanged(quint32 id);
    void onRemoved(const TrackEntry &t);

    TrackStore *m_store = nullptr;
    QVector<quint32> m_ids;
};

// Tab "Danh sách" của panel 2 (step-06 mục 11).
class TrackListTab : public QWidget
{
    Q_OBJECT
public:
    explicit TrackListTab(QWidget *parent = nullptr);

    void setTrackStore(TrackStore *store);

signals:
    // Bấm đúp một dòng: mở cửa sổ thông tin của quỹ đạo đó trên panel 1.
    void trackActivated(quint32 id);
    void plotListRequested();
    void clearPlotsRequested();
    // Thông báo bình thường cho cửa sổ "Thông báo hệ thống".
    void notice(const QString &text);

private:
    void removeSelected();
    void clearAll();

    TrackStore *m_store = nullptr;
    TrackListModel *m_model = nullptr;
    QTableView *m_view = nullptr;
};
