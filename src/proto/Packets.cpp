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

// --------------------------------------------- lệnh điều khiển mức kỹ sư

namespace CmdAdmin {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        6,      // cuongdo_video
        1,      // diemdau_video
        0,      // kenh_video
        0,      // cuaso_nguong
        1005,   // deltatx  (1,005)
        0,      // tx1_phase_100
        0,      // tx1_phase_50
        900,    // tx1_amp_100
        900,    // tx1_amp_50
        0,      // tx2_phase_100
        0,      // tx2_phase_50
        900,    // tx2_amp_100
        900,    // tx2_amp_50
        0,      // do_cs
        0,      // view_iq
        0,      // calib_onoff
        0, 0, 0, 0, 0,   // calib_bu_* — điều khiển nằm ở gói CMD_ADMIN_BUPHABD
        0,      // ak_test
        1000,   // ak_gain_kc
        1000,   // ak_gain_kp
        1000,   // ak_gain_kt
        1005,   // deltatx_f2
        1005,   // deltatx_f3
        0, 0, 0
    };
    return d;
}

} // namespace CmdAdmin

namespace CmdAdminAd {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        1532,   // ad9361_ftx
        1464,   // ad9361_frx
        29,     // ad9361_gain_rx1
        29,     // ad9361_gain_rx2
        0,      // ad9361_gain_tx1
        0,      // ad9361_gain_tx2
        0,      // input_type: MHz
        0, 0
    };
    return d;
}

// Chín mục đầu xếp đúng thứ tự giá trị calib_onoff = 0..8 để tab "ADMIN" gán
// thẳng chỉ số; mục "TLKT" nằm cuối vì không có calib_onoff nào ứng với nó.
static const FreqPreset kPresets[] = {
    {"Làm việc",     1532, 1464},
    {"Calib F2 Rx",  1458, 1464},
    {"Calib F3 Rx",  1470, 1464},
    {"Calib F4 Rx",  1532, 1538},
    {"Calib F2 Tx",  1458, 1464},
    {"Calib F3 Tx",  1470, 1464},
    {"Calib Sys F2", 1458, 1464},
    {"Calib Sys F3", 1470, 1464},
    {"Calib Sys F4", 1532, 1538},
    {"TLKT",         1464,    0},   // frx = 0: giữ nguyên giá trị đang có
};

int freqPresetCount() { return int(sizeof(kPresets) / sizeof(kPresets[0])); }

const FreqPreset &freqPreset(int index)
{
    return kPresets[qBound(0, index, freqPresetCount() - 1)];
}

} // namespace CmdAdminAd

namespace CmdAdminSw {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        1,      // video_multi
        1,      // video_divi
        10,     // cx_min
        120,    // cx_max
        2,      // cx_begin
        5,      // cx_end
        0,      // ena_plot_debug
        0,      // ena_video_src
        0,      // ena_plot_src
        200,    // st_timer
        0,      // ena_print_console
        0,      // iq_src
        1,      // auto_bugps
        0, 0, 0
    };
    return d;
}

} // namespace CmdAdminSw

namespace CmdAdminOther {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        0,      // bu_goc
        0,      // giatri_bu
        0,      // plot_bu_cly
        0,      // plot_bu_pvi
        12,     // locxung
        0,      // luu_thamso
        quint32(-600),   // calib_r_m2
        0, 0, 0, 0, 0
    };
    return d;
}

} // namespace CmdAdminOther

namespace CmdAdminCalibReg {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        32768, 0,   // F2.1
        32768, 0,   // F2.2
        32768, 0,   // F3.1
        32768, 0,   // F3.2
        32768, 0,   // F4.1
        32768, 0,   // F4.2
        0, 0, 0, 0, 0
    };
    return d;
}

} // namespace CmdAdminCalibReg

namespace CmdAdminBuphabd {

const quint32 *defaults()
{
    static const quint32 d[Count] = {
        0, 0, 0,          // bù pha F2/F3/F4
        100, 100, 100,    // bù biên độ F2/F3/F4
        0, 0, 0, 0, 0
    };
    return d;
}

} // namespace CmdAdminBuphabd

namespace StatusParams {

const char *name(int index)
{
    // Bảng tên tham số của hệ thống XL MH; các ô chưa dùng đến để "NA".
    static const char *const kNames[kCount] = {
        "HS_AK", "HS_Mono_1", "Nguong_PH_1", "HS_STC",
        "tx1_phase_100", "tx1_phase_50", "tx1_amp_100", "tx1_amp_50",
        "tx2_phase_100", "tx2_phase_50", "tx2_amp_100", "tx2_amp_50",
        "HS_Mono_2",
        "bu_f2_phase", "bu_f3_phase", "bu_f4_phase",
        "bu_f2_amp", "bu_f3_amp", "bu_f4_amp",
        "calib_k3_f2_rx_amp", "calib_k3_f2_rx_phase",
        "calib_k3_f3_rx_amp", "calib_k3_f3_rx_phase",
        "calib_k3_f4_rx_amp", "calib_k3_f4_rx_phase",
        "calib_k3_f2_tx_amp", "calib_k3_f2_tx_phase",
        "calib_k3_f3_tx_amp", "calib_k3_f3_tx_phase",
        "calib_k56_f2_amp", "calib_k56_f2_phase",
        "calib_k56_f3_amp", "calib_k56_f3_phase",
        "calib_k56_f4_amp", "calib_k56_f4_phase",
        // 35..46 chưa dùng đến
        "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA",
        "gain_rx1", "gain_rx2", "loc_xung", "giatribu",
        "plot_bu_cly", "plot_bu_pvi",
        "cxmin", "cxmax", "cxbegin", "cxend",
        "timer", "calibR_m2", "calib_bu_k2_sys_f4", "Nguong_PH_2",
        "gain_tx_1", "gain_tx_2",
        "calib_bu_k2_sys_f2", "calib_bu_k2_sys_f3",
        "auto_bugps", "start_video",
        // 67..99 chưa dùng đến
        "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA",
        "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA",
        "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA", "NA",
    };
    if (index < 0 || index >= kCount)
        return "NA";
    return kNames[index];
}

} // namespace StatusParams
