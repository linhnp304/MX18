#include "proto/RawIq.h"

#include "proto/Dataframe.h"

#include <cmath>
#include <limits>

namespace RawIq {

namespace {

constexpr int kTypeWord = 0;
constexpr int kAzimuthWord = 2;

inline const char *dataWord(const char *raw, int index)
{
    return raw + (kDataOffset + index) * 4;
}

} // namespace

QString typeName(int t)
{
    switch (t) {
    case CsF2: return QStringLiteral("CS F2");
    case CsF3: return QStringLiteral("CS F3");
    case F2I:  return QStringLiteral("F2-I");
    case F2Q:  return QStringLiteral("F2-Q");
    case F3I:  return QStringLiteral("F3-I");
    case F3Q:  return QStringLiteral("F3-Q");
    default:   return QString();
    }
}

bool readHeader(const char *raw, int size, bool bigEndian, Header *out)
{
    if (!raw || size < kMinBytes)
        return false;
    const int type = int(Proto::readU32(raw + kTypeWord * 4, bigEndian) & 0x0fu);
    if (!isValidType(type))
        return false;
    out->type = type;
    out->azm4096 = int(Proto::readU32(raw + kAzimuthWord * 4, bigEndian) & 0x0fffu);
    return true;
}

void extractView(const char *raw, bool bigEndian, int startWord, qint16 *iq1, qint16 *iq2)
{
    const char *p = dataWord(raw, qBound(0, startWord, kViewStartMax));
    qint16 prev1 = 0;
    qint16 prev2 = 0;
    for (int i = 0; i < kViewPoints; ++i, p += 4) {
        const quint32 w = Proto::readU32(p, bigEndian);
        const auto a = qint16(w & 0xffffu);
        const auto b = qint16(w >> 16);
        if (!isNoise(a))
            prev1 = a;
        if (!isNoise(b))
            prev2 = b;
        iq1[i] = prev1;
        iq2[i] = prev2;
    }
}

void beamMeans(const char *raw, bool bigEndian, int startWord, int meanWords,
               double *sumMean, double *subMean)
{
    const int count = qBound(kMeanMin, meanWords, kMeanMax);
    const char *p = dataWord(raw, qBound(0, startWord, beamStartMax(count)));
    double sum = 0.0;
    double sub = 0.0;
    int nSum = 0;
    int nSub = 0;
    for (int i = 0; i < count; ++i, p += 4) {
        const quint32 w = Proto::readU32(p, bigEndian);
        const auto a = qint16(w & 0xffffu);
        const auto b = qint16(w >> 16);
        if (!isNoise(a)) {
            sum += a;
            ++nSum;
        }
        if (!isNoise(b)) {
            sub += b;
            ++nSub;
        }
    }
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    *sumMean = nSum ? sum / nSum : kNaN;
    *subMean = nSub ? sub / nSub : kNaN;
}

double toDb(double mean)
{
    if (std::isnan(mean))
        return mean;
    return 20.0 * std::log10(mean < 1.0 ? 1.0 : mean);
}

} // namespace RawIq
