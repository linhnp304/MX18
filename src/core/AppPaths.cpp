#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>

namespace AppPaths {

QString appDir()
{
    return QCoreApplication::applicationDirPath();
}

QString mapsDir()      { return appDir() + QStringLiteral("/maps/mc"); }
QString settingsDir()  { return appDir() + QStringLiteral("/settings"); }
QString resourcesDir() { return appDir() + QStringLiteral("/resources"); }
QString logsDir()      { return appDir() + QStringLiteral("/logs"); }
QString recordsDir()   { return appDir() + QStringLiteral("/records"); }

QString settingsFile(const QString &name) { return settingsDir() + QLatin1Char('/') + name; }
QString resourceFile(const QString &name) { return resourcesDir() + QLatin1Char('/') + name; }

void ensureWritableDirs()
{
    QDir d;
    d.mkpath(settingsDir());
    d.mkpath(logsDir());
    d.mkpath(recordsDir());
}

} // namespace AppPaths
