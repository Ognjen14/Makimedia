#pragma once

#include "Metadata/FileNameParser.h"
#include "Metadata/MatchScorer.h"
#include "Metadata/TmdbDtos.h"

#include <vector>

namespace MatchDecision {

struct Choice
{
    int index = -1;
    MatchScore score;

    bool isEmpty() const { return index < 0; }
};

Choice pickBest(const ParsedFileName &parsed,
                const std::vector<Makimedia::Tmdb::TmdbTitleResultDto> &candidates);

struct Retry
{
    enum Kind {
        Nothing,
        WithoutTheYear,
        TheOtherName
    };

    Kind kind = Nothing;
    ParsedFileName parsed;
    bool useYear = true;

    bool isWorthTrying() const { return kind != Nothing; }
};

Retry askAgainAnotherWay(const ParsedFileName &parsed, bool usedYear);

}
