#include "Library/SearchIndex.h"

#include "TextFold.h"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

namespace {

constexpr int kMostPeopleLendingTitles = 3;

constexpr int kLongestInitials = 6;

constexpr int kShortestForgiven = 4;

bool isArticle(const QString &word)
{
    return word == QLatin1String("the") || word == QLatin1String("a")
        || word == QLatin1String("an");
}

int kindOrder(int kind)
{
    switch (kind) {
    case CreditRecord::Director:
        return 0;
    case CreditRecord::Creator:
        return 1;
    default:
        return 2;
    }
}

}

QString SearchIndex::fold(const QString &text)
{
    const QString folded = TextFold::key(text);

    QString out;
    out.reserve(folded.size());
    bool space = true;
    for (const QChar c : folded) {
        if (c.isLetterOrNumber()) {
            out += c;
            space = false;
        } else if (c == QLatin1Char('\'') || c == QChar(0x2019)) {
            continue;
        } else if (!space) {
            out += QLatin1Char(' ');
            space = true;
        }
    }
    if (out.endsWith(QLatin1Char(' '))) {
        out.chop(1);
    }
    return out;
}

int SearchIndex::distance(const QString &a, const QString &b, int most)
{
    const int n = int(a.size());
    const int m = int(b.size());
    if (std::abs(n - m) > most) {
        return most + 1;
    }

    std::vector<int> before(m + 1);
    std::vector<int> previous(m + 1);
    std::vector<int> current(m + 1);
    for (int j = 0; j <= m; ++j) {
        previous[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        current[0] = i;
        int rowBest = current[0];
        for (int j = 1; j <= m; ++j) {
            const int cost = a.at(i - 1) == b.at(j - 1) ? 0 : 1;
            int value = std::min({previous[j] + 1, current[j - 1] + 1,
                                  previous[j - 1] + cost});
            if (i > 1 && j > 1 && a.at(i - 1) == b.at(j - 2)
                && a.at(i - 2) == b.at(j - 1)) {
                value = std::min(value, before[j - 2] + 1);
            }
            current[j] = value;
            rowBest = std::min(rowBest, value);
        }
        if (rowBest > most) {
            return most + 1;
        }
        std::swap(before, previous);
        std::swap(previous, current);
    }
    return previous[m];
}

int SearchIndex::titleScore(int tier)
{
    switch (tier) {
    case Exact:
        return 1000;
    case Prefix:
        return 900;
    case WordPrefix:
        return 800;
    case Initials:
        return 700;
    case Contains:
        return 500;
    case Typo:
        return 250;
    default:
        return 0;
    }
}

int SearchIndex::personScore(int tier)
{
    switch (tier) {
    case Exact:
        return 950;
    case Prefix:
        return 850;
    case WordPrefix:
        return 750;
    case Typo:
        return 200;
    default:
        return 0;
    }
}

SearchIndex::Text SearchIndex::textOf(const QString &value)
{
    Text text;
    text.key = fold(value);
    const QStringList words = text.key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    text.compact = words.join(QString());

    QString tail;
    for (qsizetype i = words.size() - 1; i >= 0; --i) {
        tail.prepend(words.at(i));
        text.tails.prepend(tail);
    }

    for (qsizetype i = 0; i < words.size(); ++i) {
        if (i == 0 && words.size() > 1 && isArticle(words.at(i))) {
            continue;
        }
        text.initials += words.at(i).at(0);
    }
    return text;
}

SearchIndex::Query SearchIndex::queryOf(const QString &value)
{
    Query query;
    query.key = fold(value);
    query.words = query.key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    query.compact = query.words.join(QString());
    return query;
}

bool SearchIndex::startsAnyTail(const QString &queryWord, const QStringList &tails)
{
    for (const QString &tail : tails) {
        if (tail.startsWith(queryWord)) {
            return true;
        }
    }
    return false;
}

bool SearchIndex::nearAnyTail(const QString &queryWord, const QStringList &tails)
{
    const int size = int(queryWord.size());
    for (const QString &tail : tails) {
        for (int length = size - 1; length <= size + 1; ++length) {
            if (length <= 0 || length > tail.size()) {
                continue;
            }
            if (distance(queryWord, tail.left(length), 1) <= 1) {
                return true;
            }
        }
    }
    return false;
}

int SearchIndex::match(const Text &text, const Query &query, bool initials)
{
    if (query.isEmpty() || text.compact.isEmpty()) {
        return NoMatch;
    }

    if (text.key == query.key || text.compact == query.compact) {
        return Exact;
    }

    if (text.key.startsWith(query.key) || text.compact.startsWith(query.compact)) {
        return Prefix;
    }

    bool everyWord = true;
    for (const QString &word : query.words) {
        if (!startsAnyTail(word, text.tails)) {
            everyWord = false;
            break;
        }
    }
    if (everyWord) {
        return WordPrefix;
    }

    if (initials && query.words.size() == 1 && query.compact.size() >= 2
        && query.compact.size() <= kLongestInitials
        && text.initials.startsWith(query.compact)) {
        return Initials;
    }

    if (query.compact.size() >= 2 && text.compact.contains(query.compact)) {
        return Contains;
    }

    if (query.compact.size() >= kShortestForgiven) {
        bool near = true;
        for (const QString &word : query.words) {
            const bool found = word.size() >= kShortestForgiven
                ? nearAnyTail(word, text.tails)
                : startsAnyTail(word, text.tails);
            if (!found) {
                near = false;
                break;
            }
        }
        if (near) {
            return Typo;
        }
    }

    return NoMatch;
}

void SearchIndex::setTitles(const QList<MediaRecord> &titles)
{
    m_entries.clear();
    m_entryById.clear();
    m_entries.reserve(titles.size());

    for (const MediaRecord &record : titles) {
        if (!record.isValid() || m_entryById.contains(record.id)) {
            continue;
        }
        Entry entry;
        entry.record = record;
        entry.title = textOf(record.title);
        if (!record.originalTitle.isEmpty()
            && fold(record.originalTitle) != entry.title.key) {
            entry.original = textOf(record.originalTitle);
        }
        m_entryById.insert(record.id, int(m_entries.size()));
        m_entries.append(entry);
    }
}

void SearchIndex::setCredits(const QList<Credit> &credits)
{
    m_people.clear();
    m_personByKey.clear();

    int order = 0;
    for (const Credit &credit : credits) {
        const CreditRecord &record = credit.second;
        if (record.kind == CreditRecord::Writer) {
            continue;
        }

        const QString key = fold(record.name);
        if (key.isEmpty()) {
            continue;
        }

        auto found = m_personByKey.constFind(key);
        int at = 0;
        if (found == m_personByKey.constEnd()) {
            Person person;
            person.name = record.name.trimmed();
            person.text = textOf(record.name);
            at = int(m_people.size());
            m_people.append(person);
            m_personByKey.insert(key, at);
        } else {
            at = found.value();
        }

        Person &person = m_people[at];
        if (person.profilePath.isEmpty() && !record.profilePath.isEmpty()) {
            person.profilePath = record.profilePath;
        }
        person.roles[credit.first].append({record.kind, record.role, order++});
    }
}

int SearchIndex::titleCount() const
{
    return int(m_entries.size());
}

const SearchIndex::Role *SearchIndex::leadRole(const Person &person, qint64 mediaId) const
{
    const auto found = person.roles.constFind(mediaId);
    if (found == person.roles.constEnd() || found->isEmpty()) {
        return nullptr;
    }

    const Role *lead = &found->first();
    for (const Role &role : *found) {
        if (kindOrder(role.kind) < kindOrder(lead->kind)) {
            lead = &role;
        }
    }
    return lead;
}

SearchIndex::PersonHit SearchIndex::hitOf(const Person &person, int tier) const
{
    PersonHit hit;
    hit.name = person.name;
    hit.profilePath = person.profilePath;
    hit.tier = tier;

    for (auto it = person.roles.constBegin(); it != person.roles.constEnd(); ++it) {
        if (!m_entryById.contains(it.key())) {
            continue;
        }
        hit.mediaIds.append(it.key());
        for (const Role &role : it.value()) {
            hit.acted = hit.acted || role.kind == CreditRecord::Cast;
            hit.directed = hit.directed || role.kind == CreditRecord::Director;
            hit.created = hit.created || role.kind == CreditRecord::Creator;
        }
    }
    return hit;
}

QList<SearchIndex::PersonHit> SearchIndex::people(const QString &queryText, int limit) const
{
    const Query query = queryOf(queryText);
    QList<PersonHit> hits;
    if (query.isEmpty() || limit == 0) {
        return hits;
    }

    for (const Person &person : m_people) {
        const int tier = match(person.text, query, false);
        if (tier < WordPrefix && tier != Typo) {
            continue;
        }
        PersonHit hit = hitOf(person, tier);
        if (!hit.mediaIds.isEmpty()) {
            hits.append(hit);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const PersonHit &a, const PersonHit &b) {
        if (a.tier != b.tier) {
            return a.tier > b.tier;
        }
        if (a.mediaIds.size() != b.mediaIds.size()) {
            return a.mediaIds.size() > b.mediaIds.size();
        }
        return TextFold::compare(a.name, b.name) < 0;
    });

    if (limit > 0 && hits.size() > limit) {
        hits.resize(limit);
    }
    return hits;
}

QList<SearchIndex::TitleHit> SearchIndex::titles(const QString &queryText, int limit) const
{
    const Query query = queryOf(queryText);
    QList<TitleHit> hits;
    if (query.isEmpty() || limit == 0) {
        return hits;
    }

    QHash<qint64, TitleHit> byId;
    for (const Entry &entry : m_entries) {
        int tier = match(entry.title, query, true);
        if (!entry.original.compact.isEmpty()) {
            tier = std::max(tier, match(entry.original, query, false));
        }
        if (tier == NoMatch) {
            continue;
        }
        TitleHit hit;
        hit.title = entry.record;
        hit.score = titleScore(tier);
        byId.insert(entry.record.id, hit);
    }

    if (query.compact.size() >= 3) {
        const QList<PersonHit> found = people(queryText);
        QList<PersonHit> lending;
        for (const PersonHit &person : found) {
            if (person.tier < WordPrefix || person.tier != found.first().tier) {
                break;
            }
            lending.append(person);
        }

        if (!lending.isEmpty() && lending.size() <= kMostPeopleLendingTitles) {
            for (const PersonHit &lender : std::as_const(lending)) {
                const Person &person =
                    m_people.at(m_personByKey.value(fold(lender.name)));
                const int score = lender.tier >= Prefix ? 650 : 600;

                for (const qint64 mediaId : lender.mediaIds) {
                    TitleHit &hit = byId[mediaId];
                    if (hit.score >= score) {
                        continue;
                    }
                    const Role *role = leadRole(person, mediaId);
                    hit.title = m_entries.at(m_entryById.value(mediaId)).record;
                    hit.score = score;
                    hit.person = person.name;
                    hit.personKind = role ? role->kind : CreditRecord::Cast;
                    hit.role = role ? role->role : QString();
                }
            }
        }
    }

    hits = byId.values();
    std::sort(hits.begin(), hits.end(), [this](const TitleHit &a, const TitleHit &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        const QString &aKey = m_entries.at(m_entryById.value(a.title.id)).title.key;
        const QString &bKey = m_entries.at(m_entryById.value(b.title.id)).title.key;
        const int byKey = aKey.compare(bKey);
        if (byKey != 0) {
            return byKey < 0;
        }
        return a.title.year < b.title.year;
    });

    if (limit > 0 && hits.size() > limit) {
        hits.resize(limit);
    }
    return hits;
}

SearchIndex::PersonHit SearchIndex::person(const QString &name) const
{
    const auto found = m_personByKey.constFind(fold(name));
    if (found == m_personByKey.constEnd()) {
        return PersonHit();
    }
    return hitOf(m_people.at(found.value()), NoMatch);
}

QList<SearchIndex::TitleHit> SearchIndex::titlesOf(const QString &name) const
{
    QList<TitleHit> hits;
    const auto found = m_personByKey.constFind(fold(name));
    if (found == m_personByKey.constEnd()) {
        return hits;
    }

    const Person &person = m_people.at(found.value());
    for (auto it = person.roles.constBegin(); it != person.roles.constEnd(); ++it) {
        const auto entry = m_entryById.constFind(it.key());
        if (entry == m_entryById.constEnd()) {
            continue;
        }
        const Role *role = leadRole(person, it.key());
        TitleHit hit;
        hit.title = m_entries.at(entry.value()).record;
        hit.person = person.name;
        hit.personKind = role ? role->kind : CreditRecord::Cast;
        hit.role = role ? role->role : QString();
        hits.append(hit);
    }

    std::sort(hits.begin(), hits.end(), [](const TitleHit &a, const TitleHit &b) {
        if (a.title.year != b.title.year) {
            return a.title.year > b.title.year;
        }
        return TextFold::compare(a.title.title, b.title.title) < 0;
    });
    return hits;
}
