#include "net/LinkConfig.h"

#include "core/AppPaths.h"
#include "core/JsonFile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

#include <utility> // std::as_const — MSVC không kéo theo qua header Qt

namespace {

const char *const kConnectFile = "connect.json";

struct DefaultRow {
    const char *category;
    int direction;
    int protocol;
    int type;
    const char *localIp;
    quint16 localPort;
    const char *remoteIp;
    quint16 remotePort;
};

// 9 dòng của docs/step-02.md cộng thêm Data-RAW của giai đoạn 3; không cho
// thêm/xoá dòng trên giao diện.
const DefaultRow kDefaultRows[] = {
    {"Video-R",      LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26801, "0.0.0.0",         0},
    {"Video-I",      LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26802, "0.0.0.0",         0},
    {"Data-Status",  LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26800, "0.0.0.0",         0},
    {"Data-RAW",     LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.168.232.154", 24018, "0.0.0.0",         0},
    {"Cmd-User",     LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26810},
    {"Cmd-Admin",    LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26811},
    {"Cmd-RebootMH", LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26911},
    {"X18-SCN",      LinkEntry::SendRecv, LinkEntry::Tcp, LinkEntry::ServerOrUnicast,   "192.168.232.154", 10555, "0.0.0.0",         0},
    {"X18-VQ",       LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.173.53.168",  10790, "192.173.53.69",   10770},
    {"SCH-VQ",       LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.173.53.168",  10770, "192.173.53.68",   10790},
};

quint16 portFrom(const QJsonObject &o, const QString &key)
{
    const int v = JsonFile::i(o, key, 0);
    return quint16(qBound(0, v, 65535));
}

} // namespace

LinkConfig LinkConfig::defaults()
{
    LinkConfig cfg;
    for (const DefaultRow &r : kDefaultRows) {
        LinkEntry e;
        e.category = QString::fromLatin1(r.category);
        e.direction = r.direction;
        e.protocol = r.protocol;
        e.type = r.type;
        e.localIp = QString::fromLatin1(r.localIp);
        e.localPort = r.localPort;
        e.remoteIp = QString::fromLatin1(r.remoteIp);
        e.remotePort = r.remotePort;
        cfg.entries.append(e);
    }
    return cfg;
}

QStringList LinkConfig::categoryNames()
{
    QStringList names;
    for (const DefaultRow &r : kDefaultRows)
        names << QString::fromLatin1(r.category);
    return names;
}

LinkConfig LinkConfig::load(QString *error)
{
    const QString path = AppPaths::settingsFile(QString::fromLatin1(kConnectFile));

    if (!QFile::exists(path)) {
        // Chạy lần đầu (hoặc trắc thủ copy thiếu file): dựng mặc định và ghi ra.
        const LinkConfig cfg = defaults();
        cfg.save();
        return cfg;
    }

    QString readError;
    const QJsonObject root = JsonFile::read(path, &readError);
    if (!readError.isEmpty()) {
        if (error)
            *error = readError;
        return defaults();
    }

    LinkConfig cfg;
    cfg.bigEndian = JsonFile::b(root, QStringLiteral("big_endian"), true);

    const QJsonArray arr = root.value(QStringLiteral("links")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        LinkEntry e;
        e.category = JsonFile::str(o, QStringLiteral("category"), QString());
        if (e.category.isEmpty())
            continue;
        e.direction = qBound(0, JsonFile::i(o, QStringLiteral("direction"), LinkEntry::Recv), 2);
        e.protocol = qBound(0, JsonFile::i(o, QStringLiteral("protocol"), LinkEntry::Udp), 1);
        e.type = qBound(0, JsonFile::i(o, QStringLiteral("type"), LinkEntry::ServerOrUnicast), 1);
        e.localIp = JsonFile::str(o, QStringLiteral("local_ip"), QStringLiteral("0.0.0.0"));
        e.localPort = portFrom(o, QStringLiteral("local_port"));
        e.remoteIp = JsonFile::str(o, QStringLiteral("remote_ip"), QStringLiteral("0.0.0.0"));
        e.remotePort = portFrom(o, QStringLiteral("remote_port"));
        cfg.entries.append(e);
    }

    if (cfg.entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("File %1 không có dòng cấu hình nào, dùng cấu hình mặc định.")
                         .arg(path);
        return defaults();
    }
    return cfg;
}

bool LinkConfig::save() const
{
    QJsonArray arr;
    for (const LinkEntry &e : std::as_const(entries)) {
        QJsonObject o;
        o[QStringLiteral("category")] = e.category;
        o[QStringLiteral("direction")] = e.direction;
        o[QStringLiteral("protocol")] = e.protocol;
        o[QStringLiteral("type")] = e.type;
        o[QStringLiteral("local_ip")] = e.localIp;
        o[QStringLiteral("local_port")] = int(e.localPort);
        o[QStringLiteral("remote_ip")] = e.remoteIp;
        o[QStringLiteral("remote_port")] = int(e.remotePort);
        arr.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("big_endian")] = bigEndian;
    root[QStringLiteral("links")] = arr;
    return JsonFile::write(AppPaths::settingsFile(QString::fromLatin1(kConnectFile)), root);
}

const LinkEntry *LinkConfig::find(const QString &category) const
{
    for (const LinkEntry &e : entries) {
        if (e.category.compare(category, Qt::CaseInsensitive) == 0)
            return &e;
    }
    return nullptr;
}
