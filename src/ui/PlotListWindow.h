#pragma once

#include <QAbstractTableModel>
#include <QTime>
#include <QWidget>

#include <deque>

class QTableView;

// Nhật ký điểm dấu MH: mỗi gói PLOT nhận được là một dòng, chỉ đọc.
class PlotListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColStt = 0, ColTime, ColAzm, ColRange, ColMode, ColCommander, ColFlightid, ColAltitude, ColFuel,
        ColCount
    };

    // Giữ tối đa ngần này dòng (bỏ dòng cũ nhất, STT vẫn tăng tiếp) để chạy cả
    // ngày không phình bộ nhớ (analysis-results/05 💡9).
    static constexpr int kMaxRows = 10000;

    explicit PlotListModel(QObject *parent = nullptr);

    void append(const quint32 *plotFields);   // đủ Plot::Count trường
    void clear();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    static QString widestText(int column);

private:
    struct Row {
        quint64 stt;
        QTime time;
        quint32 azm, range, retmode, commander, flightid, altitude, fuel;
    };
    std::deque<Row> m_rows;
    quint64 m_nextStt = 1;
};

// Cửa sổ "Điểm dấu MH" (step-06 mục 11), mở từ nút "Danh sách điểm dấu MH" của
// tab "Danh sách". Tạo sẵn từ đầu để nhật ký ghi cả lúc cửa sổ đang đóng.
class PlotListWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PlotListWindow(QWidget *parent = nullptr);

    void addPlot(const quint32 *plotFields);

protected:
    void showEvent(QShowEvent *event) override;

private:
    PlotListModel *m_model = nullptr;
    QTableView *m_view = nullptr;
};
