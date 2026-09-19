#include "proto/Dataframe.h"

#include <QTime>

namespace Proto {

quint32 readU32(const char *p, bool bigEndian)
{
    const quint8 *b = reinterpret_cast<const quint8 *>(p);
    if (bigEndian)
        return (quint32(b[0]) << 24) | (quint32(b[1]) << 16) | (quint32(b[2]) << 8) | quint32(b[3]);
    return (quint32(b[3]) << 24) | (quint32(b[2]) << 16) | (quint32(b[1]) << 8) | quint32(b[0]);
}

void writeU32(char *p, quint32 v, bool bigEndian)
{
    quint8 *b = reinterpret_cast<quint8 *>(p);
    if (bigEndian) {
        b[0] = quint8(v >> 24); b[1] = quint8(v >> 16); b[2] = quint8(v >> 8); b[3] = quint8(v);
    } else {
        b[0] = quint8(v); b[1] = quint8(v >> 8); b[2] = quint8(v >> 16); b[3] = quint8(v >> 24);
    }
}

quint32 msOfDay()
{
    return quint32(QTime(0, 0).msecsTo(QTime::currentTime()));
}

QByteArray build(const Frame &frame, bool bigEndian)
{
    const int total = kFixedBytes + frame.data.size();
    QByteArray raw(total, Qt::Uninitialized);
    char *p = raw.data();

    writeU32(p +  0, kHeader, bigEndian);
    writeU32(p +  4, frame.category, bigEndian);
    writeU32(p +  8, quint32(total), bigEndian);
    writeU32(p + 12, frame.serial, bigEndian);
    writeU32(p + 16, frame.time, bigEndian);
    if (!frame.data.isEmpty())
        memcpy(p + 20, frame.data.constData(), size_t(frame.data.size()));
    writeU32(p + 20 + frame.data.size(), 0, bigEndian); // checksum crc32, hiện gán 0

    return raw;
}

bool parse(const QByteArray &raw, Frame *out, bool bigEndian)
{
    if (!out || raw.size() < kFixedBytes)
        return false;
    const char *p = raw.constData();
    if (readU32(p, bigEndian) != kHeader)
        return false;

    const quint32 length = readU32(p + 8, bigEndian);
    // Gói dài hơn buffer là gói cụt; dài hơn 64 KiB thì chắc chắn sai thứ tự byte.
    if (length < quint32(kFixedBytes) || length > quint32(raw.size()) || length > 65535u)
        return false;

    out->category = readU32(p + 4, bigEndian);
    out->serial = readU32(p + 12, bigEndian);
    out->time = readU32(p + 16, bigEndian);
    out->data = raw.mid(20, int(length) - kFixedBytes);
    return true;
}

int frameLength(const QByteArray &buffer, bool bigEndian)
{
    if (buffer.size() < 12)
        return 0;
    const char *p = buffer.constData();
    if (readU32(p, bigEndian) != kHeader)
        return -1; // mất đồng bộ, lớp gọi phải bỏ byte đầu đi tìm header mới
    const quint32 length = readU32(p + 8, bigEndian);
    if (length < quint32(kFixedBytes) || length > 65535u)
        return -1;
    return int(length);
}

QByteArray packFields(const quint32 *fields, int count, bool bigEndian)
{
    QByteArray data(count * 4, Qt::Uninitialized);
    for (int i = 0; i < count; ++i)
        writeU32(data.data() + i * 4, fields[i], bigEndian);
    return data;
}

bool unpackFields(const QByteArray &data, quint32 *fields, int count, bool bigEndian)
{
    if (data.size() < count * 4)
        return false;
    for (int i = 0; i < count; ++i)
        fields[i] = readU32(data.constData() + i * 4, bigEndian);
    return true;
}

} // namespace Proto
