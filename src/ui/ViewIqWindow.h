#pragma once

#include "net/RawIqStore.h"
#include "ui/IqPlots.h"

#include <QWidget>

#include <memory>

class BeamPolarView;
class BeamScopeView;
class IqScopeView;
class QButtonGroup;
class QCheckBox;
class QGroupBox;
class QLabel;
class QSpinBox;
class QStackedWidget;
class QTimer;

// Cửa sổ "ViewIQ - Vẽ cánh sóng" (docs/step-04.md), mở từ tab "ADMIN" của cửa
// sổ kỹ sư.
//
// Là cửa sổ Qt::Tool con của cửa sổ chính chứ không phải của cửa sổ kỹ sư, nên
// hai cửa sổ ngang hàng nhau: cùng nổi trên giao diện chính mà không chặn nó,
// trắc thủ vẫn chỉnh tốc độ quay ăng ten và kỹ sư vẫn đổi view_iq trong lúc
// đang vẽ.
//
// Dữ liệu không đi qua hàng đợi tín hiệu: luồng nhận "Data-RAW" tính sẵn vào
// RawIqStore, cửa sổ chỉ lấy bản chụp theo nhịp 25 hình/giây.
class ViewIqWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ViewIqWindow(std::shared_ptr<RawIqStore> store, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum DataType { DataViewIq = 0, DataBeam = 1 };

    QGroupBox *buildControls();
    QGroupBox *buildPlots();

    void setRunning(bool running);
    void setDataType(int type);
    void applyStartRange();
    void pushWindow();
    void refresh();
    void redrawBeam();

    std::shared_ptr<RawIqStore> m_store;
    RawIqStore::Snapshot m_snap;
    quint64 m_drawnView = 0;
    quint64 m_drawnBeam = 0;
    int m_drawnBeamType = 0;

    int m_dataType = DataBeam;
    bool m_dB = true;
    AutoRange m_beamRange{1.0};
    BeamTrace m_beam;

    QCheckBox *m_runBox = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_azmLabel = nullptr;
    QButtonGroup *m_dataGroup = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
    QWidget *m_dataCluster = nullptr;
    QWidget *m_viewCluster = nullptr;
    QWidget *m_meanCluster = nullptr;
    QSpinBox *m_meanSpin = nullptr;
    QSpinBox *m_startSpin = nullptr;

    QGroupBox *m_plotGroup = nullptr;
    QStackedWidget *m_stack = nullptr;
    IqScopeView *m_scope = nullptr;
    BeamPolarView *m_polar = nullptr;
    BeamScopeView *m_beamScope = nullptr;

    QTimer *m_timer = nullptr;
};
