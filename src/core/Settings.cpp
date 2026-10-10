#include "core/Settings.h"

#include "core/AppPaths.h"
#include "core/JsonFile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
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
    loadRecords();
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
    m_setups.plotHoldSec      = qBound(1, JsonFile::i(o, QStringLiteral("plot_hold_sec"), d.plotHoldSec), 60);
    m_setups.trackDropSec     = qBound(10, JsonFile::i(o, QStringLiteral("track_drop_sec"), d.trackDropSec), 600);
    m_setups.trackSizePct     = JsonFile::i(o, QStringLiteral("track_size_pct"), d.trackSizePct);
    m_setups.plotSizePct      = JsonFile::i(o, QStringLiteral("plot_size_pct"), d.plotSizePct);
    m_setups.mhTrackInit      = JsonFile::b(o, QStringLiteral("mh_track_init"), d.mhTrackInit);
    m_setups.mergeAzimuthDeg  = qBound(0.1, JsonFile::num(o, QStringLiteral("merge_azimuth_deg"), d.mergeAzimuthDeg), 30.0);
    m_setups.mergeRangeKm     = qBound(0.1, JsonFile::num(o, QStringLiteral("merge_range_km"), d.mergeRangeKm), 50.0);
    m_setups.radarLat         = JsonFile::num(o, QStringLiteral("radar_lat"), d.radarLat);
    m_setups.radarLon         = JsonFile::num(o, QStringLiteral("radar_lon"), d.radarLon);

    const QJsonObject c = o.value(QStringLiteral("colors")).toObject();
    m_setups.colors.grid       = colorFrom(c, QStringLiteral("grid"), d.colors.grid);
    m_setups.colors.trackTrail = colorFrom(c, QStringLiteral("track_trail"), d.colors.trackTrail);
    m_setups.colors.track        = colorFrom(c, QStringLiteral("track"), d.colors.track);
    m_setups.colors.trackMh      = colorFrom(c, QStringLiteral("track_mh"), d.colors.trackMh);
    m_setups.colors.trackProfile = colorFrom(c, QStringLiteral("track_profile"), d.colors.trackProfile);
    m_setups.colors.plot         = colorFrom(c, QStringLiteral("plot"), d.colors.plot);
    m_setups.colors.alarm1       = colorFrom(c, QStringLiteral("alarm_1"), d.colors.alarm1);
    m_setups.colors.alarm2       = colorFrom(c, QStringLiteral("alarm_2"), d.colors.alarm2);

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
    o[QStringLiteral("track_drop_sec")]     = m_setups.trackDropSec;
    o[QStringLiteral("track_size_pct")]     = m_setups.trackSizePct;
    o[QStringLiteral("plot_size_pct")]      = m_setups.plotSizePct;
    o[QStringLiteral("mh_track_init")]      = m_setups.mhTrackInit;
    o[QStringLiteral("merge_azimuth_deg")]  = m_setups.mergeAzimuthDeg;
    o[QStringLiteral("merge_range_km")]     = m_setups.mergeRangeKm;
    o[QStringLiteral("radar_lat")]          = m_setups.radarLat;
    o[QStringLiteral("radar_lon")]          = m_setups.radarLon;

    QJsonObject c;
    c[QStringLiteral("grid")]        = colorToHex(m_setups.colors.grid);
    c[QStringLiteral("track_trail")] = colorToHex(m_setups.colors.trackTrail);
    c[QStringLiteral("track")]         = colorToHex(m_setups.colors.track);
    c[QStringLiteral("track_mh")]      = colorToHex(m_setups.colors.trackMh);
    c[QStringLiteral("track_profile")] = colorToHex(m_setups.colors.trackProfile);
    c[QStringLiteral("plot")]          = colorToHex(m_setups.colors.plot);
    c[QStringLiteral("alarm_1")]       = colorToHex(m_setups.colors.alarm1);
    c[QStringLiteral("alarm_2")]       = colorToHex(m_setups.colors.alarm2);
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

namespace {

// Khoá giai đoạn 6 của setupadmin.json, theo thứ tự ghi ra file.
const char *const kAdminKeys[] = {
    "vq_range_change", "vq_output_p18m", "vq_sac", "vq_sic", "vq_sector_source",
    "vq_send_tre", "vq_site_height_m",
    "mh_init_scans", "mh_speed_min_mps", "mh_speed_max_mps", "mh_extrapolate_scans",
    "mh_scan_period_s", "mh_window_azimuth_deg", "mh_window_range_km", "mh_track_id_start",
};

} // namespace

void Settings::loadSetupAdmin()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("setupadmin.json"));
    const QJsonObject o = readChecked(path);
    m_engineerPassword = JsonFile::str(o, QStringLiteral("engineer_password"),
                                       QStringLiteral("X18"));

    const VqSetup dv;
    m_vq.rangeChange = qBound(0.1, JsonFile::num(o, QStringLiteral("vq_range_change"), dv.rangeChange), 10.0);
    m_vq.outputP18m  = JsonFile::b(o, QStringLiteral("vq_output_p18m"), dv.outputP18m);
    m_vq.sac         = qBound(0, JsonFile::i(o, QStringLiteral("vq_sac"), dv.sac), 255);
    m_vq.sic         = qBound(0, JsonFile::i(o, QStringLiteral("vq_sic"), dv.sic), 255);
    const QString source = JsonFile::str(o, QStringLiteral("vq_sector_source"), QStringLiteral("VIDEO_R"));
    m_vq.sectorFromVideoI = source.compare(QStringLiteral("VIDEO_I"), Qt::CaseInsensitive) == 0;
    if (!m_vq.sectorFromVideoI && source.compare(QStringLiteral("VIDEO_R"), Qt::CaseInsensitive) != 0) {
        m_loadErrors.append(QStringLiteral("setupadmin.json: vq_sector_source \"%1\" không hợp lệ "
                                           "(VIDEO_R hoặc VIDEO_I), dùng VIDEO_R.").arg(source));
    }
    m_vq.sendTre     = JsonFile::b(o, QStringLiteral("vq_send_tre"), dv.sendTre);
    m_vq.siteHeightM = qBound(-1000, JsonFile::i(o, QStringLiteral("vq_site_height_m"), dv.siteHeightM), 9000);

    const MhTrackerSetup dm;
    MhTrackerSetup &m = m_mhTracker;
    m.initScans        = qBound(1, JsonFile::i(o, QStringLiteral("mh_init_scans"), dm.initScans), 10);
    m.speedMinMps      = qBound(0.0, JsonFile::num(o, QStringLiteral("mh_speed_min_mps"), dm.speedMinMps), 1000.0);
    m.speedMaxMps      = qBound(m.speedMinMps, JsonFile::num(o, QStringLiteral("mh_speed_max_mps"), dm.speedMaxMps), 3000.0);
    m.extrapolateScans = qBound(0, JsonFile::i(o, QStringLiteral("mh_extrapolate_scans"), dm.extrapolateScans), 20);
    m.scanPeriodS      = qBound(1.0, JsonFile::num(o, QStringLiteral("mh_scan_period_s"), dm.scanPeriodS), 60.0);
    m.windowAzimuthDeg = qBound(0.1, JsonFile::num(o, QStringLiteral("mh_window_azimuth_deg"), dm.windowAzimuthDeg), 30.0);
    m.windowRangeKm    = qBound(0.1, JsonFile::num(o, QStringLiteral("mh_window_range_km"), dm.windowRangeKm), 50.0);
    m.trackIdStart     = qBound(1, JsonFile::i(o, QStringLiteral("mh_track_id_start"), dm.trackIdStart), 4000);

    // Ghi lại khi thiếu khoá: kỹ sư mở file là thấy đủ các tham số để sửa.
    bool complete = QFile::exists(path);
    for (const char *key : kAdminKeys)
        complete = complete && o.contains(QLatin1String(key));
    if (!complete)
        saveSetupAdmin();
}

void Settings::saveSetupAdmin()
{
    // Đọc lại file trước khi ghi: các tab Admin/AD/SW/Other/Params của cửa sổ
    // kỹ sư sẽ thêm khoá riêng ở giai đoạn sau, đừng xoá mất của nhau.
    QJsonObject o = JsonFile::read(AppPaths::settingsFile(QStringLiteral("setupadmin.json")));
    o[QStringLiteral("engineer_password")] = m_engineerPassword;

    o[QStringLiteral("vq_range_change")]  = m_vq.rangeChange;
    o[QStringLiteral("vq_output_p18m")]   = m_vq.outputP18m;
    o[QStringLiteral("vq_sac")]           = m_vq.sac;
    o[QStringLiteral("vq_sic")]           = m_vq.sic;
    o[QStringLiteral("vq_sector_source")] = m_vq.sectorFromVideoI ? QStringLiteral("VIDEO_I")
                                                                  : QStringLiteral("VIDEO_R");
    o[QStringLiteral("vq_send_tre")]      = m_vq.sendTre;
    o[QStringLiteral("vq_site_height_m")] = m_vq.siteHeightM;

    const MhTrackerSetup &m = m_mhTracker;
    o[QStringLiteral("mh_init_scans")]         = m.initScans;
    o[QStringLiteral("mh_speed_min_mps")]      = m.speedMinMps;
    o[QStringLiteral("mh_speed_max_mps")]      = m.speedMaxMps;
    o[QStringLiteral("mh_extrapolate_scans")]  = m.extrapolateScans;
    o[QStringLiteral("mh_scan_period_s")]      = m.scanPeriodS;
    o[QStringLiteral("mh_window_azimuth_deg")] = m.windowAzimuthDeg;
    o[QStringLiteral("mh_window_range_km")]    = m.windowRangeKm;
    o[QStringLiteral("mh_track_id_start")]     = m.trackIdStart;
    // Ô "Khóa điều khiển" không còn lưu: mỗi lần mở cửa sổ kỹ sư đều khoá sẵn.
    o.remove(QStringLiteral("admin_locked"));
    JsonFile::write(AppPaths::settingsFile(QStringLiteral("setupadmin.json")), o);
}

// ---------------------------------------------------------- statuserror.json

void Settings::loadStatusLimits()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("statuserror.json"));
    const int errorsBefore = m_loadErrors.size();
    QJsonObject o = readChecked(path);
    const bool readFailed = m_loadErrors.size() > errorsBefore;
    const StatusLimits d;

    m_limits.min50V = JsonFile::num(o, QStringLiteral("Min50V"), d.min50V);
    m_limits.max50V = JsonFile::num(o, QStringLiteral("Max50V"), d.max50V);
    m_limits.min5V  = JsonFile::num(o, QStringLiteral("Min5V"), d.min5V);
    m_limits.max5V  = JsonFile::num(o, QStringLiteral("Max5V"), d.max5V);
    m_limits.minCs  = JsonFile::i(o, QStringLiteral("MinCs"), d.minCs);
    m_limits.maxT   = JsonFile::i(o, QStringLiteral("MaxT"), d.maxT);
    m_limits.maxH   = JsonFile::i(o, QStringLiteral("MaxH"), d.maxH);

    // Mỗi trường một đối tượng {"StDiv": .., "StAdd": ..} mang đúng tên trường
    // của gói STATUS_MH để kỹ sư đối chiếu với đặc tả khi sửa tay.
    struct ScaleKey { const char *name; StatusScale StatusLimits::*member; };
    static const ScaleKey kScales[] = {
        {"k2_nguon_50v", &StatusLimits::k2Nguon50V},
        {"k2_nguon_5v",  &StatusLimits::k2Nguon5V},
        {"k2_nguon_m5v", &StatusLimits::k2NguonM5V},
        {"k5_tx1_cs",    &StatusLimits::k5Tx1Cs},
        {"k5_tx1_hssd",  &StatusLimits::k5Tx1Hssd},
        {"k6_tx2_cs",    &StatusLimits::k6Tx2Cs},
        {"k6_tx2_hssd",  &StatusLimits::k6Tx2Hssd},
    };
    bool missing = !QFile::exists(path);
    for (const ScaleKey &k : kScales) {
        const QString key = QString::fromLatin1(k.name);
        const StatusScale &def = d.*k.member;
        StatusScale &sc = m_limits.*k.member;
        const QJsonObject so = o.value(key).toObject();
        if (!o.value(key).isObject())
            missing = true;
        sc.stDiv = JsonFile::num(so, QStringLiteral("StDiv"), def.stDiv);
        sc.stAdd = JsonFile::num(so, QStringLiteral("StAdd"), def.stAdd);
        if (!so.contains(QStringLiteral("StDiv")) || !so.contains(QStringLiteral("StAdd")))
            missing = true;
        // Chia cho 0 thì mọi giá trị thành vô cực và báo lỗi ầm ĩ: giữ mặc định.
        if (std::fabs(sc.stDiv) < 1e-9) {
            m_loadErrors.append(QStringLiteral("statuserror.json: StDiv của %1 bằng 0, dùng mặc định %2.")
                                    .arg(key).arg(def.stDiv));
            sc.stDiv = def.stDiv;
        }
    }

    // File đọc lỗi thì để nguyên cho kỹ sư sửa; thiếu file hoặc thiếu khoá hệ số
    // (file của giai đoạn trước) thì bổ sung mặc định, giữ nguyên các khoá cũ.
    if (readFailed || !missing)
        return;
    o[QStringLiteral("Min50V")] = m_limits.min50V;
    o[QStringLiteral("Max50V")] = m_limits.max50V;
    o[QStringLiteral("Min5V")]  = m_limits.min5V;
    o[QStringLiteral("Max5V")]  = m_limits.max5V;
    o[QStringLiteral("MinCs")]  = m_limits.minCs;
    o[QStringLiteral("MaxT")]   = m_limits.maxT;
    o[QStringLiteral("MaxH")]   = m_limits.maxH;
    for (const ScaleKey &k : kScales) {
        const StatusScale &sc = m_limits.*k.member;
        QJsonObject so;
        so[QStringLiteral("StDiv")] = sc.stDiv;
        so[QStringLiteral("StAdd")] = sc.stAdd;
        o[QString::fromLatin1(k.name)] = so;
    }
    JsonFile::write(path, o);
}

// --------------------------------------------------------------- records.json

void Settings::loadRecords()
{
    const QString path = AppPaths::settingsFile(QStringLiteral("records.json"));
    const int errorsBefore = m_loadErrors.size();
    QJsonObject o = readChecked(path);
    const bool readFailed = m_loadErrors.size() > errorsBefore;
    const RecordSetup d;

    // Ngưỡng dưới nhỏ để thử ngắt file / xoá file cũ mà không phải chờ hàng giờ.
    // write_max_size kẹp dưới 4 GB vì header ghi dung lượng file bằng u32;
    // write_max_time kẹp dưới 1000 giờ vì thời điểm trong file là ms u32.
    m_records.fullVideoR = JsonFile::b(o, QStringLiteral("write_full_video_r"), d.fullVideoR);
    m_records.fullVideoI = JsonFile::b(o, QStringLiteral("write_full_video_i"), d.fullVideoI);
    m_records.rawIq      = JsonFile::b(o, QStringLiteral("write_raw_iq"), d.rawIq);
    m_records.maxSizeGb  = qBound(0.001, JsonFile::num(o, QStringLiteral("write_max_size"), d.maxSizeGb), 3.9);
    m_records.maxTimeH   = qBound(0.001, JsonFile::num(o, QStringLiteral("write_max_time"), d.maxTimeH), 1000.0);
    m_records.totalCapGb = qMax(0.01, JsonFile::num(o, QStringLiteral("total_cap"), d.totalCapGb));

    static const char *const kKeys[] = {"write_full_video_r", "write_full_video_i", "write_raw_iq",
                                        "write_max_size", "write_max_time", "total_cap"};
    bool missing = false;
    for (const char *k : kKeys)
        missing = missing || !o.contains(QLatin1String(k));
    // Như statuserror.json: file hỏng thì để nguyên cho kỹ sư sửa, thiếu file
    // hay thiếu khoá thì bổ sung mặc định, giữ nguyên giá trị đang có.
    if (readFailed || !missing)
        return;
    o[QStringLiteral("write_full_video_r")] = m_records.fullVideoR;
    o[QStringLiteral("write_full_video_i")] = m_records.fullVideoI;
    o[QStringLiteral("write_raw_iq")]       = m_records.rawIq;
    o[QStringLiteral("write_max_size")]     = m_records.maxSizeGb;
    o[QStringLiteral("write_max_time")]     = m_records.maxTimeH;
    o[QStringLiteral("total_cap")]          = m_records.totalCapGb;
    JsonFile::write(path, o);
}
