#include "ui/MainWindow.h"

#include "core/AppPaths.h"
#include "core/Settings.h"
#include "net/DataLink.h"
#include "net/PingService.h"
#include "proto/Dataframe.h"
#include "proto/Packets.h"
#include "ui/AmplitudeView.h"
#include "ui/ControlPanel.h"
#include "ui/ControlTab.h"
#include "ui/EngineerWindow.h"
#include "ui/MapView.h"
#include "ui/MhStatusPopup.h"
#include "ui/Popups.h"
#include "ui/SettingsTab.h"
#include "ui/SetupDialogs.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QMessageBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("MX18 — Phần mềm trắc thủ ra đa"));

    // Đọc cấu hình cổng gửi/nhận trước khi dựng giao diện: cửa sổ kỹ sư và nút
    // "Kết nối hệ thống" đều dùng chung bản cấu hình này.
    m_linkConfig = LinkConfig::load(&m_linkConfigError);

    buildUi();
    wireSignals();
    startPing();

    notify(QStringLiteral("Khởi động phần mềm MX18."));
    reportConfigErrors();
}

MainWindow::~MainWindow()
{
    if (m_ping)
        m_ping->stop();
    if (m_links)
        m_links->stop();
}

void MainWindow::reportConfigErrors()
{
    // File cấu hình hỏng thì phần mềm vẫn chạy với giá trị mặc định; trắc thủ
    // đọc dòng [Lỗi] rồi tìm kỹ sư sửa file.
    const QStringList errors = Settings::instance().takeLoadErrors();
    for (const QString &e : errors)
        notify(e, true);
    if (!m_linkConfigError.isEmpty())
        notify(m_linkConfigError, true);
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
    m_mhPopup = new MhStatusPopup(central);
    auto *scnPopup = new PlaceholderPopup(QStringLiteral("Trạng thái SCN"),
                                          QStringLiteral("Nội dung trạng thái SCN\n(giai đoạn sau)"), central);
    auto *svrPopup = new PlaceholderPopup(QStringLiteral("Trạng thái SVR"),
                                          QStringLiteral("Nội dung trạng thái SVR\n(giai đoạn sau)"), central);
    m_radarPopup = new RadarCenterPopup(central);

    m_popups.resize(StatusPanel::PopupCount);
    m_popups[StatusPanel::Notify] = m_notifyPopup;
    m_popups[StatusPanel::Network] = m_networkPopup;
    m_popups[StatusPanel::MhStatus] = m_mhPopup;
    m_popups[StatusPanel::ScnStatus] = scnPopup;
    m_popups[StatusPanel::SvrStatus] = svrPopup;
    m_popups[StatusPanel::RadarCenter] = m_radarPopup;

    m_networkPopup->setNodes(Settings::instance().netNodes());
    m_radarPopup->setCenter(Settings::instance().setups().radarLat,
                            Settings::instance().setups().radarLon);

    m_colorDialog = new ColorSetupDialog(this);
    m_engineerWindow = new EngineerWindow(this);

    m_links = new LinkManager(this);
    m_links->setConfig(m_linkConfig);

    m_angleTimer = new QTimer(this);
    m_angleTimer->setInterval(100);

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

    ControlTab *ctrl = m_controlPanel->controlTab();
    connect(ctrl, &ControlTab::engineerRequested, this, &MainWindow::openEngineerWindow);
    connect(ctrl, &ControlTab::lockChanged, this, [this](bool unlocked) {
        notify(unlocked ? QStringLiteral("Đã mở khóa điều khiển.")
                        : QStringLiteral("Đã khóa điều khiển."));
    });
    connect(ctrl, &ControlTab::unlockDenied, this, [this] {
        notify(QStringLiteral("Phải bấm \"Kết nối hệ thống\" trong tab Cài đặt trước khi mở "
                              "khóa điều khiển."), true);
    });
    connect(ctrl, &ControlTab::cmdAtChanged, this, &MainWindow::sendCmdAt);
    connect(ctrl, &ControlTab::cmdUserChanged, this, &MainWindow::sendCmdUser);

    connect(m_engineerWindow, &EngineerWindow::configSaved, this,
            [this](const QString &message) { notify(message); });

    connect(m_links, &LinkManager::frameReceived, this, &MainWindow::onFrame);
    connect(m_links, &LinkManager::message, this,
            [this](const QString &text, bool isError) { notify(text, isError); });

    connect(m_angleTimer, &QTimer::timeout, this, [this] {
        m_statusPanel->setSweepAngles(m_hasAngles, m_azRd, m_azMh);
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

// --------------------------------------------------- gói tin nhận được

void MainWindow::onFrame(quint32 category, quint32 serial, const QByteArray &data)
{
    const bool be = m_linkConfig.bigEndian;

    switch (category) {
    case Proto::CatVideoR: {
        if (data.size() < 4)
            return;
        m_azRd = Proto::readU32(data.constData(), be) * Video::kAzimuthLsb;
        m_hasAngles = true;
        m_mapView->setRadarSweep(m_azRd);
        return;
    }
    case Proto::CatVideoI: {
        if (data.size() < Video::kDataBytes)
            return;
        m_azMh = Proto::readU32(data.constData(), be) * Video::kAzimuthLsb;
        m_hasAngles = true;
        const QByteArray video = data.mid(4, Video::kSamples);
        m_mapView->setMhSweep(m_azMh, video);
        m_controlPanel->amplitudeView()->setTrace(video);
        return;
    }
    case Proto::CatStatusMh: {
        quint32 fields[StatusMh::Count] = {0};
        if (!Proto::unpackFields(data, fields, StatusMh::Count, be))
            return;
        m_mhPopup->setStatus(fields, serial);
        m_statusPanel->setPopupState(StatusPanel::MhStatus,
                                     m_mhPopup->hasError() ? StatusPanel::Error : StatusPanel::Ok);
        return;
    }
    case Proto::CatCmdUserBack: {
        quint32 fields[CmdUser::Count] = {0};
        if (!Proto::unpackFields(data, fields, CmdUser::Count, be))
            return;
        // Chỉ khi MH báo đang nối phát mới coi công suất thấp là lỗi.
        m_mhPopup->setTransmitOn(fields[CmdUser::Noiphat] == 1);
        return;
    }
    default:
        return;
    }
}

void MainWindow::sendCmdAt()
{
    if (!m_links->isRunning())
        return;
    const QByteArray data = Proto::packFields(m_controlPanel->controlTab()->cmdAtFields(),
                                              CmdAt::Count, m_linkConfig.bigEndian);
    m_links->send(QStringLiteral("Cmd-Admin"), Proto::CatCmdAt, data);
}

void MainWindow::sendCmdUser()
{
    if (!m_links->isRunning())
        return;
    const QByteArray data = Proto::packFields(m_controlPanel->controlTab()->cmdUserFields(),
                                              CmdUser::Count, m_linkConfig.bigEndian);
    m_links->send(QStringLiteral("Cmd-User"), Proto::CatCmdUser, data);
}

void MainWindow::openEngineerWindow()
{
    // Mật khẩu chỉ hỏi một lần cho mỗi lần chạy phần mềm.
    if (!EngineerWindow::passwordAccepted()) {
        EngineerPasswordDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        EngineerWindow::rememberPassword();
        notify(QStringLiteral("Đã mở quyền điều khiển mức kỹ sư."));
    }
    m_engineerWindow->show();
    m_engineerWindow->raise();
    m_engineerWindow->activateWindow();
}

// ------------------------------------------------------------- kết nối

void MainWindow::setConnected(bool connected)
{
    m_connected = connected;
    m_controlPanel->settingsTab()->setConnected(connected);
    m_controlPanel->controlTab()->setSystemConnected(connected);

    if (connected) {
        m_links->start();
        m_angleTimer->start();
    } else {
        m_links->stop();
        m_angleTimer->stop();
        m_hasAngles = false;
        m_mapView->clearVideo();
        m_controlPanel->amplitudeView()->clearTrace();
        m_mhPopup->clearStatus();
        m_statusPanel->setSweepAngles(false, 0.0, 0.0);
    }

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
