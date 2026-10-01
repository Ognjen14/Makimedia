#include "Metadata/PosterUrlResolver.h"

#include "Metadata/TmdbClient.h"

#include "MmLog.h"

namespace Makimedia::Tmdb
{
namespace
{

constexpr int ConfigurationRetryIntervalMs = 5000;

}

PosterUrlResolver::PosterUrlResolver(TmdbClient &tmdbClient, QObject *parent)
    : QObject(parent)
    , m_tmdbClient(tmdbClient)
{
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(ConfigurationRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &PosterUrlResolver::fetchConfiguration);

    fetchConfiguration();
}

void PosterUrlResolver::fetchConfiguration()
{
    const auto id = m_tmdbClient.configuration(
        [this](TmdbClient::ConfigurationResult result) {
            if (auto *configuration = std::get_if<TmdbConfigurationResponseDto>(&result))
            {
                m_configuration = configuration->images;
                m_loaded = true;
                MM_LOG_I() << "PosterUrlResolver loaded TMDB image configuration";
                emit configurationLoaded();
                return;
            }

            MM_LOG_W() << "PosterUrlResolver failed to load TMDB image configuration,"
                          " retrying shortly";
            m_retryTimer.start();
        });
    Q_UNUSED(id)
}

bool PosterUrlResolver::isReady() const noexcept
{
    return m_loaded;
}

QString PosterUrlResolver::resolveUrl(ImageKind kind,
                                      const QString &imagePath,
                                      int requestedPixelWidth) const
{
    if (!m_loaded || imagePath.isEmpty())
    {
        return QString();
    }

    const QUrl url = TmdbImageUrl::imageUrl(m_configuration,
                                            kind,
                                            imagePath.toStdString(),
                                            requestedPixelWidth);
    return url.isValid() ? url.toString() : QString();
}

}
