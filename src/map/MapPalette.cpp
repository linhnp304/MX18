#include "map/MapPalette.h"

#include <QtGlobal>

namespace {

QColor mix(const QColor &a, const QColor &b, double t)
{
    return QColor(int(qRound(a.red() + (b.red() - a.red()) * t)),
                  int(qRound(a.green() + (b.green() - a.green()) * t)),
                  int(qRound(a.blue() + (b.blue() - a.blue()) * t)));
}

} // namespace

MapPalette MapPalette::forBrightness(int level)
{
    const double t = (qBound(1, level, 10) - 1) / 9.0;

    MapPalette p;
    p.sea       = mix(QColor(0x0d, 0x0d, 0x0e), QColor(0x84, 0x84, 0x88), t);
    p.land      = mix(QColor(0x0d, 0x0d, 0x0d), QColor(0x7b, 0x7f, 0x7b), t);
    p.province  = mix(QColor(0x1e, 0x1e, 0x1e), QColor(0x67, 0x6b, 0x67), t);
    p.river     = mix(QColor(0x24, 0x7d, 0x99), QColor(0x44, 0x9e, 0xba), t);
    p.airRoute  = mix(QColor(0x4b, 0x6f, 0x49), QColor(0x80, 0xa2, 0x7f), t);
    p.placeText = mix(QColor(0x4a, 0x4a, 0x4a), QColor(0x10, 0x10, 0x10), t);
    p.routeText = mix(QColor(0x6b, 0x6b, 0x64), QColor(0x2a, 0x2c, 0x2a), t);

    p.coast              = QColor(0xb8, 0x90, 0x4c);
    p.airportVn          = QColor(0xe0, 0x10, 0x10);
    p.airportForeign     = QColor(0x3b, 0x5f, 0xc0);
    p.airportTextVn      = QColor(0xff, 0x8c, 0x1a);
    p.airportTextForeign = QColor(0x6e, 0x9c, 0xe8);
    return p;
}
