#pragma once

#include "TmdbDtos.h"
#include "TmdbImageUrl.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace Makimedia::Tmdb
{

class TmdbClient;

class PosterUrlResolver final : public QObject
{
    Q_OBJECT

public:
    explicit PosterUrlResolver(TmdbClient &tmdbClient, QObject *parent = nullptr);

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] QString resolveUrl(ImageKind kind,
                                     const QString &imagePath,
                                     int requestedPixelWidth) const;

signals:
    void configurationLoaded();

private:
    void fetchConfiguration();

    TmdbClient &m_tmdbClient;
    TmdbImageConfigurationDto m_configuration;
    bool m_loaded{false};
    QTimer m_retryTimer;
};

}
