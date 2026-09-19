#include "proto/Packets.h"

#include <QString>

namespace CmdAt {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        0,  // anten_onoff
        1,  // anten_speed
        0,  // anten_sync
        0, 0, 0, 0, 0
    };
    return d;
}

} // namespace CmdAt

namespace CmdUser {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        0,    // cd_lamviec
        0,    // gia_bd
        0,    // gia_bn
        0,    // xungkich
        1,    // nguon_pvi
        0,    // vantoc_giaquay
        0,    // nguon_cs
        0,    // icode1
        1,    // mode
        1,    // rcode1
        1,    // icode3
        1,    // rcode3
        1,    // key_m2
        0,    // noiphat
        0,    // kenhphu
        1,    // cs_phat
        0,    // cd_phat
        70,   // azm1
        90,   // azm2
        250,  // azm3
        270,  // azm4
        1,    // cn_kdb
        0,    // cn_ak
        1024, // hs_ak
        1,    // monopulse
        30,   // nguong_monopulse
        90,   // nguong_xungdon
        1,    // hs_stc
        90,   // nguong_monopulse_2
        130,  // nguong_xungdon_2
        0, 0, 0
    };
    return d;
}

} // namespace CmdUser

namespace StatusMh {

QString keyTimeText(quint32 keyTime)
{
    // Ngày giờ nén trong một trường 32 bit theo công thức của hệ thống MH:
    // 17 bit thấp là giây trong ngày, các bit trên là ngày/tháng/năm.
    const quint32 temp = keyTime & 0x1ffffu;
    const quint32 ss = temp % 60;
    const quint32 hh = temp / 3600;
    const quint32 mm = (temp - ss - 3600 * hh) / 60;
    const quint32 yy = (keyTime >> 26) & 0x1fu;
    const quint32 MM = (keyTime >> 22) & 0xfu;
    const quint32 dd = (keyTime >> 17) & 0x1fu;

    const auto pad = [](quint32 v) {
        return QStringLiteral("%1").arg(v, 2, 10, QLatin1Char('0'));
    };
    return QStringLiteral("%1/%2/%3 %4:%5:%6")
        .arg(pad(yy), pad(MM), pad(dd), pad(hh), pad(mm), pad(ss));
}

double gpsHeading(quint32 gpsStatus)
{
    return (gpsStatus & 0x0fffu) * 360.0 / 4096.0;
}

bool gpsFixGood(quint32 gpsStatus)
{
    const quint32 signal = (gpsStatus >> 23) & 0x01u;
    const quint32 latlng = (gpsStatus >> 22) & 0x01u;
    const quint32 diff   = (gpsStatus >> 20) & 0x03u;
    return signal == 1 && latlng == 1 && diff == 2;
}

} // namespace StatusMh
