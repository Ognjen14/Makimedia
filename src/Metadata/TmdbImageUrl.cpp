#include "Metadata/TmdbImageUrl.h"

#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>

namespace Makimedia::Tmdb
{
namespace
{

struct ImageSize final
{
    int width;
    const char *name;
};

constexpr std::array<ImageSize, 6> PosterSizes{{
    {92, "w92"},
    {154, "w154"},
    {185, "w185"},
    {342, "w342"},
    {500, "w500"},
    {780, "w780"}
}};

constexpr std::array<ImageSize, 3> StillSizes{{
    {92, "w92"},
    {185, "w185"},
    {300, "w300"}
}};

constexpr std::array<ImageSize, 3> BackdropSizes{{
    {300, "w300"},
    {780, "w780"},
    {1280, "w1280"}
}};

constexpr std::array<ImageSize, 3> ProfileSizes{{
    {45, "w45"},
    {185, "w185"},
    {632, "h632"}
}};

struct SizeLadder final
{
    const ImageSize *first;
    std::size_t count;

    const ImageSize *begin() const { return first; }
    const ImageSize *end() const { return first + count; }
    const ImageSize &widest() const { return first[count - 1]; }
};

SizeLadder ladderFor(ImageKind kind)
{
    switch (kind)
    {
    case ImageKind::Still:
        return {StillSizes.data(), StillSizes.size()};
    case ImageKind::Backdrop:
        return {BackdropSizes.data(), BackdropSizes.size()};
    case ImageKind::Profile:
        return {ProfileSizes.data(), ProfileSizes.size()};
    case ImageKind::Poster:
    default:
        return {PosterSizes.data(), PosterSizes.size()};
    }
}

const std::vector<std::string> &listedSizes(
    const TmdbImageConfigurationDto &configuration,
    ImageKind kind)
{
    switch (kind)
    {
    case ImageKind::Still:
        return configuration.stillSizes;
    case ImageKind::Backdrop:
        return configuration.backdropSizes;
    case ImageKind::Profile:
        return configuration.profileSizes;
    case ImageKind::Poster:
    default:
        return configuration.posterSizes;
    }
}

bool isListed(const std::vector<std::string> &listed, const char *name)
{
    return std::find(listed.cbegin(), listed.cend(), name) != listed.cend();
}

std::optional<std::string> selectSize(
    const TmdbImageConfigurationDto &configuration,
    ImageKind kind,
    int requestedPixelWidth)
{
    const SizeLadder ladder = ladderFor(kind);
    const std::vector<std::string> &listed = listedSizes(configuration, kind);
    const int safeRequestedWidth = std::max(1, requestedPixelWidth);

    for (const ImageSize &size : ladder)
    {
        if (size.width >= safeRequestedWidth && isListed(listed, size.name))
        {
            return std::string(size.name);
        }
    }

    for (auto size = ladder.end(); size != ladder.begin();)
    {
        --size;
        if (isListed(listed, size->name))
        {
            return std::string(size->name);
        }
    }

    return std::nullopt;
}

QUrl buildImageUrl(const TmdbImageConfigurationDto &configuration,
                   const std::string &size,
                   const std::string &imagePath)
{
    QUrl url(QString::fromStdString(configuration.secureBaseUrl), QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QStringLiteral("https"))
    {
        return {};
    }

    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
    {
        path.append(QLatin1Char('/'));
    }

    path.append(QString::fromStdString(size));
    path.append(QLatin1Char('/'));

    QString relative = QString::fromStdString(imagePath);
    while (relative.startsWith(QLatin1Char('/')))
    {
        relative.remove(0, 1);
    }
    path.append(relative);

    url.setPath(path);
    url.setQuery(QString{});
    url.setFragment(QString{});
    return url;
}

}

std::optional<std::string> TmdbImageUrl::selectPosterSize(
    const TmdbImageConfigurationDto &configuration,
    int requestedPixelWidth)
{
    return selectSize(configuration, ImageKind::Poster, requestedPixelWidth);
}

std::vector<int> TmdbImageUrl::widths(ImageKind kind)
{
    std::vector<int> result;
    for (const ImageSize &size : ladderFor(kind))
    {
        result.push_back(size.width);
    }
    return result;
}

int TmdbImageUrl::widthFor(ImageKind kind, int requestedPixelWidth)
{
    const SizeLadder ladder = ladderFor(kind);
    const int safeRequestedWidth = std::max(1, requestedPixelWidth);

    for (const ImageSize &size : ladder)
    {
        if (size.width >= safeRequestedWidth)
        {
            return size.width;
        }
    }
    return ladder.widest().width;
}

QUrl TmdbImageUrl::imageUrl(
    const TmdbImageConfigurationDto &configuration,
    ImageKind kind,
    const std::string &imagePath,
    int requestedPixelWidth)
{
    const auto size = selectSize(configuration, kind, requestedPixelWidth);
    if (!size.has_value()
        || configuration.secureBaseUrl.empty()
        || imagePath.empty())
    {
        return {};
    }

    return buildImageUrl(configuration, *size, imagePath);
}

QUrl TmdbImageUrl::posterUrl(
    const TmdbImageConfigurationDto &configuration,
    const std::string &posterPath,
    int requestedPixelWidth)
{
    return imageUrl(configuration, ImageKind::Poster, posterPath, requestedPixelWidth);
}

QUrl TmdbImageUrl::stillUrl(
    const TmdbImageConfigurationDto &configuration,
    const std::string &stillPath,
    int requestedPixelWidth)
{
    return imageUrl(configuration, ImageKind::Still, stillPath, requestedPixelWidth);
}

QUrl TmdbImageUrl::backdropUrl(
    const TmdbImageConfigurationDto &configuration,
    const std::string &backdropPath,
    int requestedPixelWidth)
{
    return imageUrl(configuration, ImageKind::Backdrop, backdropPath,
                    requestedPixelWidth);
}

}
