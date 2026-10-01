#include "Metadata/MatchDecision.h"

#include <cstddef>

using Makimedia::Tmdb::TmdbTitleResultDto;

namespace MatchDecision {

Choice pickBest(const ParsedFileName &parsed,
                const std::vector<TmdbTitleResultDto> &candidates)
{
    Choice choice;

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const MatchScore score = MatchScorer::score(parsed, candidates[index]);
        if (choice.isEmpty() || score.value > choice.score.value) {
            choice.index = static_cast<int>(index);
            choice.score = score;
        }
    }

    return choice;
}

Retry askAgainAnotherWay(const ParsedFileName &parsed, bool usedYear)
{
    Retry retry;

    if (usedYear) {
        retry.kind = Retry::WithoutTheYear;
        retry.parsed = parsed;
        retry.useYear = false;
        return retry;
    }

    if (!parsed.alternativeTitle.isEmpty()) {
        retry.kind = Retry::TheOtherName;
        retry.parsed = parsed;
        retry.parsed.title = parsed.alternativeTitle;
        retry.parsed.alternativeTitle.clear();
        retry.useYear = true;

        if (parsed.year > 0
            && retry.parsed.title.contains(QString::number(parsed.year))) {
            retry.parsed.year = 0;
            retry.useYear = false;
        }

        return retry;
    }

    return retry;
}

}
