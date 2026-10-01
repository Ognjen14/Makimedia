#include "Metadata/MakimediaTmdbConfig.h"

#include "AppSettings.h"
#include "Config/ApiKeys.h"
#include "MmLog.h"

#include <QByteArray>

namespace Makimedia::Tmdb
{

MakimediaTmdbConfig::MakimediaTmdbConfig(const AppSettings &settings) noexcept
    : m_settings(settings)
{
}

MakimediaTmdbConfig::~MakimediaTmdbConfig() = default;

QString MakimediaTmdbConfig::bundledKey()
{
    if (Config::TmdbApiKeyLength == 0) {
        return QString();
    }

    QByteArray decoded;
    decoded.reserve(static_cast<int>(Config::TmdbApiKeyLength));
    for (std::size_t i = 0; i < Config::TmdbApiKeyLength; ++i) {
        const unsigned char maskByte =
            Config::TmdbApiKeyMask[i % sizeof(Config::TmdbApiKeyMask)];
        decoded.append(static_cast<char>(Config::TmdbApiKeyObfuscated[i] ^ maskByte));
    }
    return QString::fromUtf8(decoded);
}

TmdbRequestAccess MakimediaTmdbConfig::requestAccess() const
{
    TmdbRequestAccess access;
    access.baseUrl = QUrl(QStringLiteral("https://api.themoviedb.org/3/"));

    access.apiKey = bundledKey();

    return access;
}

bool MakimediaTmdbConfig::hasKey() const
{
    return !requestAccess().apiKey.isEmpty();
}

}
