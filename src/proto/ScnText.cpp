#include "proto/ScnText.h"

namespace ScnText {

namespace {

// SW1 gửi bằng StreamWriter.WriteLine(): mỗi lần gửi tự thêm \r\n. MX18 chạy cả
// trên Linux nên ghi tường minh.
QByteArray line(const char *text)
{
    return QByteArray(text) + "\r\n";
}

// Hai bảng của SW1: so khớp nguyên văn cả dòng (sau khi bỏ khoảng trắng hai đầu).
struct Exact {
    const char *command;
    const char *reply;
};

// Nhóm "chưa giải mã được ý nghĩa" (SendAnser_Unknow) — nhận "!…&" trả "#…&".
const Exact kUnknownMeaning[] = {
    {"!scnrzcorr 1188105 &", "#scnrzcorr 1188105 0 &"},
    {"!scnrzcorr 0 &",       "#scnrzcorr 0 0 &"},
    {"!scnrpcorr 0 &",       "#scnrpcorr 0 0 &"},
    {"!scnrtsp 0 &",         "#scnrtsp 0 &"},
    {"!scnesen 0 &",         "#scnesen 0 &"},
    {"!scnsect 0 &",         "#scnsect 0 &"},
};

Reply known(const char *reply)
{
    Reply r;
    r.text = line(reply);
    r.known = true;
    return r;
}

} // namespace

QByteArray startBlock()
{
    // Hai lần WriteLine của SW1: khối chính (nối bằng \r\n bên trong) và
    // "#status 1 &".
    return line("#uptime 1623 0 &\r\n"
                "#freeram 8314880 0 &\r\n"
                "#macaddr \"0:5:f4: 1:6:4f\" 0 &\r\n"
                "#scnuiact 0 0 &\r\n"
                "#scnvolt \"ALL\" {0 0} \"v24\" {24126 0} \"v12\" {11001 0} \"v5\" {5065 0} \"v3p3\" {3271 0} &\r\n"
                "#scnv110ac 0 &\r\n"
                "#scnconsig 0 &\r\n"
                "#scnrzcorr 1188105 0 &\r\n"
                "#scnrpcorr")
        + line("#status 1 &");
}

Reply answer(const QByteArray &raw, State *state)
{
    const QByteArray cmd = raw.trimmed();

    // --- nhóm "đã giải mã được ý nghĩa" (SendAnser_Know), đúng thứ tự của SW1
    if (cmd == "!scnrpmode 1 &") {
        state->scnrpmode = "1";
        return known("#scnrpmode 1 0 &");
    }
    if (cmd == "!scnrpmode 3 &") {
        state->scnrpmode = "3";
        return known("#scnrpmode 11 0 &");
    }
    if (cmd == "!scnrpband 0 &") {
        state->scnrpband = "0";
        return known("#scnrpband 0 0 &");
    }
    if (cmd == "!scnrpband 1 &") {
        state->scnrpband = "1";
        return known("#scnrpband 9 0 &");
    }
    if (cmd == "!scnselrprz 1 &") {
        state->scnselrprz = "1";
        return known("#scnselrprz 1 0 &");
    }
    if (cmd == "!scnselrprz 0 &") {
        state->scnselrprz = "0";
        return known("#scnselrprz 0 0 &");
    }
    if (cmd.startsWith("!scnrprun &") || cmd.startsWith("!scnrprun 1 &"))
        return known("#scnrprun 0 0 0 0 0 0 0 &");
    if (cmd == "!scnrptx 0 &")
        return known("#scnrptx 0 0 0 0 0 0 &");
    if (cmd == "!scnrptx 1 &")
        return known("#scnrptx 0 1 0 0 0 0 &");
    if (cmd == "!corestate &")
        return known("#corestate 4 &");
    if (cmd.startsWith("!status &")) {
        Reply r = known("#status 1 &");
        r.sendStart = true;
        return r;
    }
    if (cmd.startsWith("!start &")) {
        Reply r = known("#start 1 &");
        r.sendStart = true;
        return r;
    }
    if (cmd == "!console &") {
        Reply r = known("#console 0 &");
        r.sendStart = true;
        return r;
    }
    if (cmd.startsWith("!name &"))
        return known("#name \"SCN\" &");
    if (cmd.startsWith("!version &")) {
        // Chuỗi thứ hai tự kết thúc bằng \r\n rồi WriteLine thêm một lần nữa,
        // nên sau "#version 2 &" có một dòng rỗng — giữ nguyên như SW1.
        Reply r = known("#version 1 &");
        r.text += line("#version 3 \"bl\" \"2.1.0.11\" &\r\n"
                       "#version 3 \"os\" \"4.0.0.10\" &\r\n"
                       "#version 3 \"kr\" \"2.6.29.6 - rt24\" &\r\n"
                       "#version 3 \"fs\" \"4.0.0.14\" &\r\n"
                       "#version 3 \"rt\" \"1.4.7\" &\r\n"
                       "#version 2 &\r\n");
        return r;
    }
    if (cmd.startsWith("!keepalive")) {
        // Dạng PC gửi là "!keepalive X N &": SW1 bỏ 13 ký tự đầu không kiểm tra.
        // Dòng ngắn hơn thì SW1 ném lỗi làm chết luồng; ở đây lấy phần sau tên
        // lệnh để vẫn trả lời được.
        const QByteArray rest = cmd.size() >= 13 ? cmd.mid(13) : cmd.mid(10).trimmed();
        Reply r;
        r.known = true;
        r.text = "#keepalive 2 " + rest + "\r\n";
        QByteArray n = rest;
        if (n.endsWith('&'))
            n.chop(1);
        r.keepalive = n.trimmed();
        return r;
    }

    for (const Exact &e : kUnknownMeaning) {
        if (cmd == e.command)
            return known(e.reply);
    }
    return Reply();
}

QByteArray infBlock(const State &state)
{
    return "*scnrpmode " + state.scnrpmode + " 0 &\r\n"
        + "*scnrpband " + state.scnrpband + " 0 &\r\n"
        + "*scnselrprz " + state.scnselrprz + " 0 &\r\n"
        + "*scnrprun 0 8 " + state.prPower + " 1 0 0 0 &\r\n"   // PR: Ready, Error, TestRun, Alert, Power
        + line("*scnvid 0x0080 0 0 0 &")                        // Video Buffer, Vol, Threshold
        + line("*scnrzrun 0 0 0 0 &")                           // RZ Ready, 110V, 120V
        + line("*scnuists 0 109209 0 55095 0 55009 0 55163 0 &");
}

QByteArray azimuthLine(const QByteArray &azimuth)
{
    return "*scnuiact " + azimuth + " 0 &\r\n";
}

} // namespace ScnText
