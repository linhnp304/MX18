#include "core/Settings.h"

#include "core/AppPaths.h"
#include "core/JsonFile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

QString colorToHex(const QColor &c) { return c.name(QColor::HexRgb).toUpper(); }

QColor colorFrom(const QJsonObject &o, const QString &key, const QColor &def)
{
    const QColor c(JsonFile::str(o, key, QString()));
    return c.isValid() ? c : def;
}

} // namespace

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

void Settings::load()
{
    m_loadErrors.clear();
    loadSwInfo();
    loadSetups();
    loadNetNodes();
    loadSetupAdmin();
    loadStatusLimits();
}

QStringList Settings::takeLoadErrors()
{
    QStringList errors;
    errors.swap(m_loadErrors);
    return errors;
}

QJsonObject Settings::readChecked(const QString &path)
{
    QString error;
    const QJsonObject o = JsonFile::read(path, &error);
    if (!error.isEmpty())
        m_loadErrors.append(error);
    return o;
}

// ---------------------------------------------------------------- swinfo.json

void Settings::loadSwInfo()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("swinfo.json"));
    const QJsonObject o = readChecked(path);
    const SwInfo def;
    m_swInfo.line0 = JsonFile::str(o, QStringLiteral("info_line0"), def.line0);
    m_swInfo.line1 = JsonFile::str(o, QStringLiteral("info_line1"), def.line1);
    m_swInfo.line2 = JsonFile::str(o, QStringLiteral("info_line2"), def.line2);
    if (!QFile::exists(path))
        saveSwInfo();
}

void Settings::saveSwInfo()
{
    QJsonObject o;
    o[QStringLiteral("info_line0")] = m_swInfo.line0;
    o[QStringLiteral("info_line1")] = m_swInfo.line1;
    o[QStringLiteral("info_line2")] = m_swInfo.line2;
    JsonFile::write(AppPaths::settingsFile(QStringLiteral("swinfo.json")), o);
}

// ---------------------------------------------------------------- setups.json

void Settings::loadSetups()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("setups.json"));
    const QJsonObject o = readChecked(path);
    const Setups d;

    m_setups.showMap          = JsonFile::b(o, QStringLiteral("show_map"), d.showMap);
    m_setups.showAirRoutes    = JsonFile::b(o, QStringLiteral("show_air_routes"), d.showAirRoutes);
    m_setups.showAirports     = JsonFile::b(o, QStringLiteral("show_airports"), d.showAirports);
    m_setups.brightness       = qBound(1, JsonFile::i(o, QStringLiteral("map_brightness"), d.brightness), 10);
    m_setups.videoFade        = qBound(0, JsonFile::i(o, QStringLiteral("video_fade"), d.videoFade), 10);
    m_setups.trackHistory     = qBound(0, JsonFile::i(o, QStringLiteral("track_history"), d.trackHistory), 100);
    m_setups.trailStyle       = qBound(0, JsonFile::i(o, QStringLiteral("trail_style"), d.trailStyle), 1);
    m_setups.showTrackProfile = JsonFile::b(o, QStringLiteral("show_track_profile"), d.showTrackProfile);
    m_setups.showPlotInfo     = JsonFile::b(o, QStringLiteral("show_plot_info"), d.showPlotInfo);
    m_setups.rangeRingMode    = qBound(0, JsonFile::i(o, QStringLiteral("range_ring_mode"), d.rangeRingMode), 3);
    m_setups.azimuthMode      = qBound(0, JsonFile::i(o, QStringLiteral("azimuth_mode"), d.azimuthMode), 3);
    m_setups.plotHoldSec      = qBound(1, JsonFile::i(o, QStringLiteral("plot_hold_sec"), d.plotHoldSec), 600);
    m_setups.trackSizePct     = JsonFile::i(o, QStringLiteral("track_size_pct"), d.trackSizePct);
    m_setups.plotSizePct      = JsonFile::i(o, QStringLiteral("plot_size_pct"), d.plotSizePct);
    m_setups.radarLat         = JsonFile::num(o, QStringLiteral("radar_lat"), d.radarLat);
    m_setups.radarLon         = JsonFile::num(o, QStringLiteral("radar_lon"), d.radarLon);

    const QJsonObject c = o.value(QStringLiteral("colors")).toObject();
    m_setups.colors.grid       = colorFrom(c, QStringLiteral("grid"), d.colors.grid);
    m_setups.colors.trackTrail = colorFrom(c, QStringLiteral("track_trail"), d.colors.trackTrail);
    m_setups.colors.track      = colorFrom(c, QStringLiteral("track"), d.colors.track);
    m_setups.colors.plot       = colorFrom(c, QStringLiteral("plot"), d.colors.plot);

    if (!QFile::exists(path))
        saveSetups();
}

void Settings::saveSetups()
{
    QJsonObject o;
    o[QStringLiteral("show_map")]           = m_setups.showMap;
    o[QStringLiteral("show_air_routes")]    = m_setups.showAirRoutes;
    o[QStringLiteral("show_airports")]      = m_setups.showAirports;
    o[QStringLiteral("map_brightness")]     = m_setups.brightness;
    o[QStringLiteral("video_fade")]         = m_setups.videoFade;
    o[QStringLiteral("track_history")]      = m_setups.trackHistory;
    o[QStringLiteral("trail_style")]        = m_setups.trailStyle;
    o[QStringLiteral("show_track_profile")] = m_setups.showTrackProfile;
    o[QStringLiteral("show_plot_info")]     = m_setups.showPlotInfo;
    o[QStringLiteral("range_ring_mode")]    = m_setups.rangeRingMode;
    o[QStringLiteral("azimuth_mode")]       = m_setups.azimuthMode;
    o[QStringLiteral("plot_hold_sec")]      = m_setups.plotHoldSec;
    o[QStringLiteral("track_size_pct")]     = m_setups.trackSizePct;
    o[QStringLiteral("plot_size_pct")]      = m_setups.plotSizePct;
    o[QStringLiteral("radar_lat")]          = m_setups.radarLat;
    o[QStringLiteral("radar_lon")]          = m_setups.radarLon;

    QJsonObject c;
    c[QStringLiteral("grid")]        = colorToHex(m_setups.colors.grid);
    c[QStringLiteral("track_trail")] = colorToHex(m_setups.colors.trackTrail);
    c[QStringLiteral("track")]       = colorToHex(m_setups.colors.track);
    c[QStringLiteral("plot")]        = colorToHex(m_setups.colors.plot);
    o[QStringLiteral("colors")] = c;

    JsonFile::write(AppPaths::settingsFile(QStringLiteral("setups.json")), o);
}

// --------------------------------------------------------------- checkip.json

void Settings::loadNetNodes()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("checkip.json"));
    const QJsonObject o = readChecked(path);
    const QJsonArray arr = o.value(QStringLiteral("nodes")).toArray();

    m_netNodes.clear();
    for (const QJsonValue &v : arr) {
        const QJsonObject n = v.toObject();
        NetNode node;
        node.name = JsonFile::str(n, QStringLiteral("name"), QString());
        node.address = JsonFile::str(n, QStringLiteral("address"), QString());
        node.kind = qBound(0, JsonFile::i(n, QStringLiteral("kind"), 0), 2);
        if (!node.address.isEmpty())
            m_netNodes.append(node);
    }

    if (m_netNodes.isEmpty()) {
        m_netNodes = {
            {QStringLiteral("Host 1"), QStringLiteral("127.0.0.1"), 0},
            {QStringLiteral("Host 2"), QStringLiteral("127.0.0.1"), 1},
            {QStringLiteral("Host 3"), QStringLiteral("127.0.0.1"), 2},
        };
        saveNetNodes();
    }
}

void Settings::setNetNodes(const QVector<NetNode> &nodes)
{
    m_netNodes = nodes;
    saveNetNodes();
}

void Settings::saveNetNodes()
{
    QJsonArray arr;
    for (const NetNode &n : std::as_const(m_netNodes)) {
        QJsonObject jo;
        jo[QStringLiteral("name")] = n.name;
        jo[QStringLiteral("address")] = n.address;
        jo[QStringLiteral("kind")] = n.kind;
        arr.append(jo);
    }
    QJsonObject root;
    root[QStringLiteral("nodes")] = arr;
    JsonFile::write(AppPaths::settingsFile(QStringLiteral("checkip.json")), root);
}

// ----------------------------------------------------------- setupadmin.json

void Settings::loadSetupAdmin()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("setupadmin.json"));
    const QJsonObject o = readChecked(path);
    m_engineerPassword = JsonFile::str(o, QStringLiteral("engineer_password"),
                                       QStringLiteral("X18"));
    m_adminLocked = JsonFile::b(o, QStringLiteral("admin_locked"), true);
    if (!QFile::exists(path))
        saveSetupAdmin();
}

void Settings::saveSetupAdmin()
{
    // Đọc lại file trước khi ghi: các tab Admin/AD/SW/Other/Params của cửa sổ
    // kỹ sư sẽ thêm khoá riêng ở giai đoạn sau, đừng xoá mất của nhau.
    QJsonObject o = JsonFile::read(AppPaths::settingsFile(QStringLiteral("setupadmin.json")));
    o[QStringLiteral("engineer_password")] = m_engineerPassword;
    o[QStringLiteral("admin_locked")] = m_adminLocked;
    JsonFile::write(AppPaths::settingsFile(QStringLiteral("setupadmin.json")), o);
}

void Settings::setAdminLocked(bool locked)
{
    if (m_adminLocked == locked)
        return;
    m_adminLocked = locked;
    saveSetupAdmin();
}

// ---------------------------------------------------------- statuserror.json

void Settings::loadStatusLimits()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("statuserror.json"));
    const QJsonObject o = readChecked(path);
    const StatusLimits d;

    m_limits.min50V = JsonFile::num(o, QStringLiteral("Min50V"), d.min50V);
    m_limits.max50V = JsonFile::num(o, QStringLiteral("Max50V"), d.max50V);
    m_limits.min5V  = JsonFile::num(o, QStringLiteral("Min5V"), d.min5V);
    m_limits.max5V  = JsonFile::num(o, QStringLiteral("Max5V"), d.max5V);
    m_limits.minCs  = JsonFile::i(o, QStringLiteral("MinCs"), d.minCs);
    m_limits.maxT   = JsonFile::i(o, QStringLiteral("MaxT"), d.maxT);
    m_limits.maxH   = JsonFile::i(o, QStringLiteral("MaxH"), d.maxH);

    if (!QFile::exists(path)) {
        QJsonObject def;
        def[QStringLiteral("Min50V")] = m_limits.min50V;
        def[QStringLiteral("Max50V")] = m_limits.max50V;
        def[QStringLiteral("Min5V")]  = m_limits.min5V;
        def[QStringLiteral("Max5V")]  = m_limits.max5V;
        def[QStringLiteral("MinCs")]  = m_limits.minCs;
        def[QStringLiteral("MaxT")]   = m_limits.maxT;
        def[QStringLiteral("MaxH")]   = m_limits.maxH;
        JsonFile::write(path, def);
    }
}
