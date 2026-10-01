#pragma once

#include "Metadata/TmdbRuntimeConfig.h"

class AppSettings;

namespace Makimedia::Tmdb
{

class MakimediaTmdbConfig final : public TmdbRuntimeConfig
{
public:
    explicit MakimediaTmdbConfig(const AppSettings &settings) noexcept;
    ~MakimediaTmdbConfig() override;

    [[nodiscard]] TmdbRequestAccess requestAccess() const override;

    [[nodiscard]] bool hasKey() const;

    static QString bundledKey();

private:
    const AppSettings &m_settings;
};

}
