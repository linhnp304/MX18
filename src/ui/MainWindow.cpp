#include "ui/MainWindow.h"

#include "core/AppPaths.h"
#include "core/Settings.h"
#include "net/PingService.h"
#include "ui/ControlPanel.h"
#include "ui/ControlTab.h"
#include "ui/MapView.h"
#include "ui/Popups.h"
#include "ui/SettingsTab.h"
#include "ui/SetupDialogs.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariantAnimation>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("MX18 — Phần mềm trắc thủ ra đa"));
    buildUi();
    wireSignals();
    startPing();

    notify(QStringLiteral("Khởi động phần mềm MX18."));
}

MainWindow::~MainWindow()
{
    if (m_ping)
        m_ping->stop();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *lay = new QVBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(4);

    m_mapView = new MapView(m_splitter);
    m_controlPanel = new ControlPanel(m_splitter);
    m_controlPanel->setMinimumWidth(240);

    m_splitter->addWidget(m_mapView);
    m_splitter->addWidget(m_controlPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    // Panel 1 chiếm 75% chiều ngang, panel 2 chiếm 25%.
    m_splitter->setSizes({750, 250});

    m_statusPanel = new StatusPanel(central);

    lay->addWidget(m_splitter, 1);
    lay->addWidget(m_statusPanel);
    setCentralWidget(central);

    // Popup là con của central widget nên hiệu ứng trượt nằm gọn trong cửa sổ.
    m_notifyPopup = new NotifyPopup(central);
    m_networkPopup = new NetworkPopup(central);
    auto *mhPopup = new PlaceholderPopup(QStringLiteral("Trạng thái MH"),
                                         QStringLiteral("Nội dung trạng thái MH\n(giai đoạn sau)"), central);
    auto *scnPopup = new PlaceholderPopup(QStringLiteral("Trạng thái SCN"),
                                          QStringLiteral("Nội dung trạng thái SCN\n(giai đoạn sau)"), central);
    auto *svrPopup = new PlaceholderPopup(QStringLiteral("Trạng thái SVR"),
                                          QStringLiteral("Nội dung trạng thái SVR\n(giai đoạn sau)"), central);
    m_radarPopup = new RadarCenterPopup(central);

    m_popups.resize(StatusPanel::PopupCount);
    m_popups[StatusPanel::Notify] = m_notifyPopup;
    m_popups[StatusPanel::Network] = m_networkPopup;
    m_popups[StatusPanel::MhStatus] = mhPopup;
    m_popups[StatusPanel::ScnStatus] = scnPopup;
    m_popups[StatusPanel::SvrStatus] = svrPopup;
    m_popups[StatusPanel::RadarCenter] = m_radarPopup;

    m_networkPopup->setNodes(Settings::instance().netNodes());
    m_radarPopup->setCenter(Settings::instance().setups().radarLat,
                            Settings::instance().setups().radarLon);

    m_colorDialog = new ColorSetupDialog(this);

    m_panelAnim = new QVariantAnimation(this);
    m_panelAnim->setDuration(200);
    m_panelAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_controlPanel->settingsTab()->setConnected(false);
}

void MainWindow::wireSignals()
{
    connect(m_mapView, &MapView::cursorGeoChanged, m_statusPanel, &StatusPanel::setCursorInfo);

    connect(m_statusPanel, &StatusPanel::popupToggled, this, &MainWindow::togglePopup);
    connect(m_statusPanel, &StatusPanel::panelToggleRequested, this, &MainWindow::toggleControlPanel);

    SettingsTab *tab = m_controlPanel->settingsTab();
    connect(tab, &SettingsTab::displayChanged, this, [this] { m_mapView->refreshSettings(); });
    connect(tab, &SettingsTab::colorSetupRequested, this, [this] {
        m_colorDialog->loadFromSettings();
        m_colorDialog->show();
        m_colorDialog->raise();
        m_colorDialog->activateWindow();
    });
    connect(tab, &SettingsTab::connectToggled, this, &MainWindow::setConnected);
    connect(tab, &SettingsTab::exitRequested, this, &MainWindow::requestExit);

    connect(m_colorDialog, &ColorSetupDialog::applied, this, [this] {
        m_mapView->refreshSettings();
        notify(QStringLiteral("Đã áp dụng thiết lập màu sắc và tham số hiển thị."));
    });

    connect(m_controlPanel->controlTab(), &ControlTab::engineerRequested, this, [this] {
        EngineerPasswordDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        notify(QStringLiteral("Mở cửa sổ điều khiển mức kỹ sư."));
        EngineerDialog eng(this);
        eng.exec();
    });
    connect(m_controlPanel->controlTab(), &ControlTab::lockChanged, this, [this](bool unlocked) {
        notify(unlocked ? QStringLiteral("Đã mở khóa điều khiển.")
                        : QStringLiteral("Đã khóa điều khiển."));
    });

    connect(m_radarPopup, &RadarCenterPopup::applyRequested, this, [this](double lat, double lon) {
        Setups &s = Settings::instance().setups();
        s.radarLat = lat;
        s.radarLon = lon;
        Settings::instance().saveSetups();
        m_mapView->setRadarCenter(lat, lon);
        notify(QStringLiteral("Tâm đài chuyển về %1 - %2.")
                   .arg(QString::number(lat, 'f', 6), QString::number(lon, 'f', 6)));
    });
    connect(m_radarPopup, &RadarCenterPopup::gpsRequested, this, [this] {
        notify(QStringLiteral("Chưa nhận được dữ liệu GPS (chức năng hoàn thiện ở giai đoạn sau)."),
               true);
    });
    connect(m_radarPopup, &RadarCenterPopup::recenterRequested, this, [this] {
        m_mapView->centerOnRadar();
    });

    connect(m_notifyPopup, &NotifyPopup::cleared, this, [this] {
        m_statusPanel->setPopupState(StatusPanel::Notify, StatusPanel::Idle);
    });

    for (int i = 0; i < m_popups.size(); ++i) {
        SlidePopup *popup = m_popups.at(i);
        if (!popup)
            continue;
        connect(popup, &SlidePopup::closed, this, [this, i] {
            m_statusPanel->setPopupChecked(StatusPanel::PopupId(i), false);
            if (m_openPopup == i)
                m_openPopup = -1;
            if (m_pendingPopup >= 0) {
                const int next = m_pendingPopup;
                m_pendingPopup = -1;
                openPopup(next);
            }
        });
    }

    connect(m_panelAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const int w = v.toInt();
        const int total = m_splitter->width() - m_splitter->handleWidth();
        m_splitter->setSizes({qMax(0, total - w), w});
    });
}

void MainWindow::startPing()
{
    // Ping chạy ngay khi mở phần mềm, không phụ thuộc nút "Kết nối".
    m_ping = new PingService(this);
    connect(m_ping, &PingService::nodeStateChanged, this, [this](int index, bool alive) {
        m_networkPopup->setNodeState(index, alive);
    });
    connect(m_ping, &PingService::overallLevelChanged, this, [this](int level) {
        const StatusPanel::StateColor c = (level == 2) ? StatusPanel::Error
                                        : (level == 1) ? StatusPanel::Warn
                                                       : StatusPanel::Ok;
        m_statusPanel->setPopupState(StatusPanel::Network, c);
    });
    m_ping->start(Settings::instance().netNodes());
}

bool MainWindow::loadMapData()
{
    const bool ok = m_mapData.load(AppPaths::mapsDir());
    if (!ok) {
        notify(m_mapData.lastError(), true);
    } else {
        notify(QStringLiteral("Đã nạp nền bản đồ số: %1 lớp, %2 sân bay, %3 địa danh.")
                   .arg(m_mapData.layers().size())
                   .arg(m_mapData.airports().size())
                   .arg(m_mapData.places().size()));
    }
    m_mapView->setMapData(ok ? &m_mapData : nullptr);
    return ok;
}

void MainWindow::notify(const QString &message, bool isError)
{
    m_notifyPopup->append(message, isError);
    if (m_openPopup != StatusPanel::Notify) {
        m_statusPanel->setPopupState(StatusPanel::Notify,
                                     isError ? StatusPanel::Error : StatusPanel::Ok);
    }
}

// ------------------------------------------------------------------ popup

void MainWindow::togglePopup(int id)
{
    if (m_openPopup == id) {
        closeCurrentPopup(-1);
        return;
    }
    if (m_openPopup >= 0) {
        // Đóng cửa sổ đang mở trước rồi mới mở cửa sổ mới.
        closeCurrentPopup(id);
        return;
    }
    openPopup(id);
}

void MainWindow::closeCurrentPopup(int pendingId)
{
    m_pendingPopup = pendingId;
    if (m_openPopup >= 0 && m_openPopup < m_popups.size() && m_popups.at(m_openPopup))
        m_popups.at(m_openPopup)->closePopup();
}

void MainWindow::openPopup(int id)
{
    if (id < 0 || id >= m_popups.size() || !m_popups.at(id))
        return;

    if (id == StatusPanel::Notify)
        m_statusPanel->setPopupState(StatusPanel::Notify, StatusPanel::Idle);

    m_openPopup = id;
    m_statusPanel->setPopupChecked(StatusPanel::PopupId(id), true);
    placePopup(id);
}

void MainWindow::placePopup(int id)
{
    SlidePopup *popup = m_popups.at(id);
    popup->adjustSize();

    QWidget *central = centralWidget();
    const int margin = 8;
    const int bottom = m_statusPanel->y() - margin;
    const int y = qMax(margin, bottom - popup->height());

    const bool fromRight = (id == StatusPanel::RadarCenter);
    const int x = fromRight ? qMax(margin, central->width() - popup->width() - margin) : margin;
    popup->openAt(QPoint(x, y));
}

// ------------------------------------------------------------- panel 2

void MainWindow::toggleControlPanel()
{
    const QList<int> sizes = m_splitter->sizes();
    const int current = sizes.size() > 1 ? sizes.at(1) : 0;

    m_panelAnim->stop();
    m_panelAnim->disconnect(SIGNAL(finished()));

    if (!m_panelHidden) {
        m_savedPanelWidth = qMax(240, current);
        m_controlPanel->setMinimumWidth(0);
        m_panelAnim->setStartValue(current);
        m_panelAnim->setEndValue(0);
        connect(m_panelAnim, &QVariantAnimation::finished, this, [this] {
            m_controlPanel->hide();
        }, Qt::SingleShotConnection);
    } else {
        m_controlPanel->show();
        m_panelAnim->setStartValue(0);
        m_panelAnim->setEndValue(m_savedPanelWidth);
        connect(m_panelAnim, &QVariantAnimation::finished, this, [this] {
            m_controlPanel->setMinimumWidth(240);
        }, Qt::SingleShotConnection);
    }

    m_panelHidden = !m_panelHidden;
    m_statusPanel->setPanelHidden(m_panelHidden);
    m_panelAnim->start();
}

// ------------------------------------------------------------- kết nối

void MainWindow::setConnected(bool connected)
{
    m_connected = connected;
    m_controlPanel->settingsTab()->setConnected(connected);

    // Chưa kết nối thì các trạng thái MH/SCN/SVR để màu trắng xám.
    const StatusPanel::StateColor c = connected ? StatusPanel::Ok : StatusPanel::Idle;
    m_statusPanel->setPopupState(StatusPanel::MhStatus, c);
    m_statusPanel->setPopupState(StatusPanel::ScnStatus, c);
    m_statusPanel->setPopupState(StatusPanel::SvrStatus, c);

    notify(connected ? QStringLiteral("Bắt đầu nhận/gửi dữ liệu.")
                     : QStringLiteral("Đã dừng nhận/gửi dữ liệu."));
}

void MainWindow::requestExit()
{
    if (m_connected) {
        notify(QStringLiteral("Phải dừng kết nối trước khi thoát phần mềm."), true);
        return;
    }
    close();
}

// ------------------------------------------------------------- sự kiện

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_openPopup >= 0)
        placePopup(m_openPopup);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Tiện cho lúc chạy thử trên máy phát triển; máy trắc thủ luôn toàn màn hình.
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_connected) {
        QMessageBox::warning(this, QStringLiteral("Đang kết nối"),
                             QStringLiteral("Phải dừng kết nối trước khi thoát phần mềm."));
        event->ignore();
        return;
    }
    if (m_ping)
        m_ping->stop();
    Settings::instance().saveSetups();
    event->accept();
}
