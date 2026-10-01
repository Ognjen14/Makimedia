#pragma once

#include "Metadata/FileNameParser.h"
#include "Metadata/TmdbDtos.h"

#include <QString>

struct MatchScore
{
    int value = 0;
    QString reason;

    bool isConfident() const { return value >= 80; }
    bool isSuggestion() const { return value >= 45 && value < 80; }
    bool isRejected() const { return value < 45; }
};

namespace MatchScorer {

QString normaliseTitle(const QString &title);

int titleSimilarity(const QString &parsedTitle, const QString &candidateTitle);

MatchScore score(const ParsedFileName &parsed,
                 const Makimedia::Tmdb::TmdbTitleResultDto &candidate);

}
