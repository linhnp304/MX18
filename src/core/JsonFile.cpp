#include "core/JsonFile.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>

namespace JsonFile {

QJsonObject read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();
    return doc.object();
}

bool write(const QString &path, const QJsonObject &obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    // QSaveFile để cấu hình không bị cụt nếu mất điện giữa lúc ghi.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return f.commit();
}

double num(const QJsonObject &o, const QString &key, double def)
{
    const QJsonValue v = o.value(key);
    return v.isDouble() ? v.toDouble() : def;
}

int i(const QJsonObject &o, const QString &key, int def)
{
    const QJsonValue v = o.value(key);
    return v.isDouble() ? v.toInt() : def;
}

bool b(const QJsonObject &o, const QString &key, bool def)
{
    const QJsonValue v = o.value(key);
    return v.isBool() ? v.toBool() : def;
}

QString str(const QJsonObject &o, const QString &key, const QString &def)
{
    const QJsonValue v = o.value(key);
    return v.isString() ? v.toString() : def;
}

} // namespace JsonFile
