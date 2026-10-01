#pragma once

#include <QLatin1String>
#include <QString>

namespace VideoTraits {

inline bool isHdr(const QString &primaries, const QString &gamma)
{
    const bool perceptual = gamma == QLatin1String("pq")
        || gamma == QLatin1String("hlg");
    const bool wide = primaries == QLatin1String("bt.2020");
    return perceptual && wide;
}

}
