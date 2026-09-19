#pragma once

#include "core/Settings.h"
#include "ui/SlidePopup.h"

#include <QVector>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;

// Cửa sổ "Thông báo hệ thống".
class NotifyPopup : public SlidePopup
{
    Q_OBJECT
public:
    explicit NotifyPopup(QWidget *parent);

    void append(const QString &message, bool isError);

signals:
    void cleared();

private:
    QTextEdit *m_log = nullptr;
    QPushButton *m_clearBtn = nullptr;
};

// Cửa sổ "Trạng thái kết nối mạng".
class NetworkPopup : public SlidePopup
{
    Q_OBJECT
public:
    explicit NetworkPopup(QWidget *parent);

    void setNodes(const QVector<NetNode> &nodes);
    void setNodeState(int index, bool alive);

private:
    void fitRows(int visibleRows);

    QTableWidget *m_table = nullptr;
};

// Cửa sổ "Tọa độ tâm đài".
class RadarCenterPopup : public SlidePopup
{
    Q_OBJECT
public:
    explicit RadarCenterPopup(QWidget *parent);

    void setCenter(double lat, double lon);

signals:
    void applyRequested(double lat, double lon);
    void gpsRequested();
    void recenterRequested();

private:
    QLineEdit *m_lat = nullptr;
    QLineEdit *m_lon = nullptr;
};

// Khung sẵn cho các cửa sổ trạng thái MH / SCN / SVR, nội dung ở giai đoạn sau.
class PlaceholderPopup : public SlidePopup
{
    Q_OBJECT
public:
    PlaceholderPopup(const QString &title, const QString &note, QWidget *parent);
};
