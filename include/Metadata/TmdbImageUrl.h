#pragma once

#include "TmdbDtos.h"

#include <QUrl>

#include <optional>
#include <string>
#include <vector>

namespace Makimedia::Tmdb
{

enum class ImageKind
{
    Poster,
    Still,
    Backdrop,
    Profile
};

class TmdbImageUrl final
{
public:
    [[nodiscard]] static std::optional<std::string> selectPosterSize(
        const TmdbImageConfigurationDto &configuration,
        int requestedPixelWidth);

    [[nodiscard]] static std::vector<int> widths(ImageKind kind);

    [[nodiscard]] static int widthFor(ImageKind kind, int requestedPixelWidth);

    [[nodiscard]] static QUrl imageUrl(
        const TmdbImageConfigurationDto &configuration,
        ImageKind kind,
        const std::string &imagePath,
        int requestedPixelWidth);

    [[nodiscard]] static QUrl posterUrl(
        const TmdbImageConfigurationDto &configuration,
        const std::string &posterPath,
        int requestedPixelWidth);

    [[nodiscard]] static QUrl stillUrl(
        const TmdbImageConfigurationDto &configuration,
        const std::string &stillPath,
        int requestedPixelWidth);

    [[nodiscard]] static QUrl backdropUrl(
        const TmdbImageConfigurationDto &configuration,
        const std::string &backdropPath,
        int requestedPixelWidth);
};

}
