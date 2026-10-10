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
    int format;
};

// 9 dòng của docs/step-02.md, Data-RAW của giai đoạn 3, hai dòng UDP của luồng
// SCN (giai đoạn 5 chốt, analysis-results file 04 mục 2) và Params-Status: hệ
// thống MH thật gửi STATUS_PARAMS + phản hồi BUPHABD ra cổng 26812 chứ không
// phải 26800 (bắt gói 2026-10-08). Không cho thêm/xoá dòng trên giao diện.
const DefaultRow kDefaultRows[] = {
    {"Video-R",      LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26801, "0.0.0.0",         0,     LinkEntry::Dataframe},
    {"Video-I",      LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26802, "0.0.0.0",         0,     LinkEntry::Dataframe},
    {"Data-Status",  LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26800, "0.0.0.0",         0,     LinkEntry::Dataframe},
    {"Params-Status", LinkEntry::Recv,    LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154", 26812, "0.0.0.0",         0,     LinkEntry::Dataframe},
    {"Data-RAW",     LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.168.232.154", 24018, "0.0.0.0",         0,     LinkEntry::RawIq},
    {"Cmd-User",     LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26810, LinkEntry::Dataframe},
    {"Cmd-Admin",    LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26811, LinkEntry::Dataframe},
    {"Cmd-RebootMH", LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ClientOrBroadcast, "192.168.232.154",     0, "192.168.232.255", 26911, LinkEntry::Dataframe},
    {"X18-SCN",      LinkEntry::SendRecv, LinkEntry::Tcp, LinkEntry::ServerOrUnicast,   "192.168.232.154", 10555, "0.0.0.0",         0,     LinkEntry::ScnText},
    {"X18-SCN-R",    LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.168.232.154", 10597, "0.0.0.0",         0,     LinkEntry::ScnCf},
    {"X18-SCN-S",    LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.168.232.154",     0, "192.168.232.1",   10613, LinkEntry::ScnCf},
    {"X18-VQ",       LinkEntry::Recv,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.173.53.168",  10790, "192.173.53.69",   10770, LinkEntry::Asterix},
    {"SCH-VQ",       LinkEntry::Send,     LinkEntry::Udp, LinkEntry::ServerOrUnicast,   "192.173.53.168",  10770, "192.173.53.68",   10790, LinkEntry::Asterix},
};

const char *const kFormatNames[LinkEntry::FormatCount] = {
    "dataframe", "raw_iq", "scn_text", "scn_cf", "asterix",
};

LinkEntry entryFrom(const DefaultRow &r)
{
    LinkEntry e;
    e.category = QString::fromLatin1(r.category);
    e.direction = r.direction;
    e.protocol = r.protocol;
    e.type = r.type;
    e.localIp = QString::fromLatin1(r.localIp);
    e.localPort = r.localPort;
    e.remoteIp = QString::fromLatin1(r.remoteIp);
    e.remotePort = r.remotePort;
    e.format = r.format;
    e.bigEndian = LinkEntry::defaultBigEndian(r.format);
    return e;
}

int indexOf(const QVector<LinkEntry> &entries, const QString &category)
{
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).category.compare(category, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

quint16 portFrom(const QJsonObject &o, const QString &key)
{
    const int v = JsonFile::i(o, key, 0);
    return quint16(qBound(0, v, 65535));
}

} // namespace

QString LinkEntry::formatName(int format)
{
    if (format < 0 || format >= FormatCount)
        return QString();
    return QString::fromLatin1(kFormatNames[format]);
}

int LinkEntry::formatFromName(const QString &name)
{
    for (int f = 0; f < FormatCount; ++f) {
        if (name.compare(QLatin1String(kFormatNames[f]), Qt::CaseInsensitive) == 0)
            return f;
    }
    return -1;
}

int LinkEntry::defaultFormat(const QString &category)
{
    for (const DefaultRow &r : kDefaultRows) {
        if (category.compare(QLatin1String(r.category), Qt::CaseInsensitive) == 0)
            return r.format;
    }
    return Dataframe;
}

LinkConfig LinkConfig::defaults()
{
    LinkConfig cfg;
    for (const DefaultRow &r : kDefaultRows)
        cfg.entries.append(entryFrom(r));
    return cfg;
}

QStringList LinkConfig::categoryNames()
{
    QStringList names;
    for (const DefaultRow &r : kDefaultRows)
        names << QString::fromLatin1(r.category);
    return names;
}

LinkConfig LinkConfig::load(QString *error, QString *note)
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

    // File của giai đoạn 2–5 có một khoá big_endian chung cho mọi dòng. Dòng của
    // hệ thống MH giữ đúng giá trị đó; các dòng X18-* trước đây không chạy được
    // nên lấy thẳng mặc định của định dạng. Thiếu cả khoá chung thì mọi dòng theo
    // mặc định (hệ thống MH little-endian), không ngầm lấy big-endian của bản cũ.
    const bool legacyKey = root.contains(QStringLiteral("big_endian"));
    const bool legacyBigEndian = JsonFile::b(root, QStringLiteral("big_endian"),
                                             LinkEntry::defaultBigEndian(LinkEntry::Dataframe));
    bool migrate = legacyKey;
    QStringList errors;

    LinkConfig cfg;
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

        e.format = LinkEntry::defaultFormat(e.category);
        if (o.contains(QStringLiteral("format"))) {
            const QString name = JsonFile::str(o, QStringLiteral("format"), QString());
            const int f = LinkEntry::formatFromName(name);
            if (f >= 0) {
                e.format = f;
            } else {
                errors << QStringLiteral("connect.json, dòng \"%1\": định dạng \"%2\" không có, "
                                         "dùng \"%3\". Định dạng hợp lệ: %4.")
                              .arg(e.category, name, LinkEntry::formatName(e.format),
                                   QStringLiteral("dataframe, raw_iq, scn_text, scn_cf, asterix"));
            }
        } else {
            migrate = true;
        }

        if (o.contains(QStringLiteral("big_endian"))) {
            e.bigEndian = JsonFile::b(o, QStringLiteral("big_endian"), LinkEntry::defaultBigEndian(e.format));
        } else {
            const bool mhLine = (e.format == LinkEntry::Dataframe || e.format == LinkEntry::RawIq);
            e.bigEndian = (mhLine && legacyKey) ? legacyBigEndian : LinkEntry::defaultBigEndian(e.format);
            migrate = true;
        }
        cfg.entries.append(e);
    }

    if (cfg.entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("File %1 không có dòng cấu hình nào, dùng cấu hình mặc định.")
                         .arg(path);
        return defaults();
    }

    // Bản cũ chưa có hai dòng UDP của luồng SCN: chèn vào ngay sau dòng đứng
    // trước nó trong bảng mặc định để tab Connect vẫn xếp theo nhóm.
    QStringList added;
    const int rowCount = int(sizeof(kDefaultRows) / sizeof(kDefaultRows[0]));
    for (int i = 0; i < rowCount; ++i) {
        const QString name = QString::fromLatin1(kDefaultRows[i].category);
        if (indexOf(cfg.entries, name) >= 0)
            continue;
        const int prev = i > 0 ? indexOf(cfg.entries, QString::fromLatin1(kDefaultRows[i - 1].category))
                               : -1;
        cfg.entries.insert(prev >= 0 ? prev + 1 : cfg.entries.size(), entryFrom(kDefaultRows[i]));
        added << name;
    }

    if (migrate || !added.isEmpty()) {
        cfg.save();
        if (note) {
            QStringList parts;
            if (migrate)
                parts << QStringLiteral("thêm định dạng và thứ tự byte cho từng dòng");
            if (!added.isEmpty())
                parts << QStringLiteral("bổ sung dòng %1").arg(added.join(QStringLiteral(", ")));
            *note = QStringLiteral("Đã cập nhật settings/connect.json: %1.")
                        .arg(parts.join(QStringLiteral("; ")));
        }
    }
    if (error && !errors.isEmpty())
        *error = errors.join(QLatin1Char('\n'));
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
        o[QStringLiteral("format")] = LinkEntry::formatName(e.format);
        o[QStringLiteral("big_endian")] = e.bigEndian;
        arr.append(o);
    }

    QJsonObject root;
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

bool LinkConfig::bigEndianFor(const QString &category) const
{
    const LinkEntry *e = find(category);
    return e ? e->bigEndian : LinkEntry::defaultBigEndian(LinkEntry::Dataframe);
}
