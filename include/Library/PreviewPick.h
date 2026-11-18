#pragma once

#include "Data/MediaRepository.h"

#include <QList>

namespace PreviewPick {

constexpr double kEarliest = 0.15;
constexpr double kLatest = 0.50;
constexpr double kUnknownLengthStart = 120.0;

double startSeconds(double durationSeconds, double chance);

int episodeIndex(const QList<EpisodeRecord> &episodes, double chance);

}
