#include "Metadata/MatchScorer.h"

#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QStringList>

using Makimedia::Tmdb::TmdbMediaType;
using Makimedia::Tmdb::TmdbTitleResultDto;

namespace {

int releaseYearOf(const std::optional<std::string> &releaseDate)
{
    if (!releaseDate.has_value() || releaseDate->size() < 4) {
        return 0;
    }
    return QString::fromStdString(releaseDate->substr(0, 4)).toInt();
}

bool isGenericStem(const QString &normalised, int year)
{
    static const QSet<QString> stems = {
        QStringLiteral("sample"),   QStringLiteral("video"),
        QStringLiteral("movie"),    QStringLiteral("film"),
        QStringLiteral("test"),     QStringLiteral("trailer"),
        QStringLiteral("clip"),     QStringLiteral("output"),
        QStringLiteral("untitled"), QStringLiteral("new"),
        QStringLiteral("temp"),     QStringLiteral("demo"),
        QStringLiteral("recording"), QStringLiteral("capture")
    };

    static const QRegularExpression trailingNumber(QStringLiteral("\\s+\\d+$"));
    QString stem = normalised;
    stem.remove(trailingNumber);

    if (stems.contains(stem)) {
        return true;
    }

    static const QRegularExpression digitsOnly(QStringLiteral("^[0-9]+$"));
    if (!digitsOnly.match(normalised).hasMatch()) {
        return false;
    }

    if (normalised.size() > 4) {
        return true;
    }

    return year == 0;
}

QSet<QString> tokensOf(const QString &normalised)
{
    QSet<QString> tokens;
    const QStringList parts =
        normalised.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        tokens.insert(part);
    }
    return tokens;
}

}

namespace MatchScorer {

namespace {

QStringList wordsOf(const QString &title)
{
    QString value = title.toLower();

    value = value.normalized(QString::NormalizationForm_KD);
    value.removeIf([](QChar c) { return c.category() == QChar::Mark_NonSpacing; });

    static const QRegularExpression apostrophes(
        QStringLiteral("['‘’ʼ´`]"));
    value.remove(apostrophes);

    value.replace(QLatin1Char('&'), QStringLiteral(" and "));

    static const QRegularExpression nonAlnum(QStringLiteral("[^a-z0-9]+"));
    value.replace(nonAlnum, QStringLiteral(" "));

    static const QRegularExpression leadingArticle(
        QStringLiteral("^(?:the|a|an)\\s+"));
    value.remove(leadingArticle);
    return value.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

QString withArabicNumerals(QStringList words)
{
    static const QHash<QString, QString> numerals = {
        {QStringLiteral("ii"), QStringLiteral("2")},
        {QStringLiteral("iii"), QStringLiteral("3")},
        {QStringLiteral("iv"), QStringLiteral("4")},
        {QStringLiteral("v"), QStringLiteral("5")},
        {QStringLiteral("vi"), QStringLiteral("6")},
        {QStringLiteral("vii"), QStringLiteral("7")},
        {QStringLiteral("viii"), QStringLiteral("8")},
        {QStringLiteral("ix"), QStringLiteral("9")},
        {QStringLiteral("x"), QStringLiteral("10")}
    };

    for (QString &word : words) {
        const auto arabic = numerals.constFind(word);
        if (arabic != numerals.constEnd()) {
            word = arabic.value();
        }
    }

    return words.join(QLatin1Char(' '));
}

int spelledOutLength(const QStringList &words)
{
    return words.join(QLatin1Char(' ')).size();
}

QStringList withNumberWords(QStringList words)
{
    static const QHash<QString, QString> numbers = {
        {QStringLiteral("one"), QStringLiteral("1")},
        {QStringLiteral("two"), QStringLiteral("2")},
        {QStringLiteral("three"), QStringLiteral("3")},
        {QStringLiteral("four"), QStringLiteral("4")},
        {QStringLiteral("five"), QStringLiteral("5")},
        {QStringLiteral("six"), QStringLiteral("6")},
        {QStringLiteral("seven"), QStringLiteral("7")},
        {QStringLiteral("eight"), QStringLiteral("8")},
        {QStringLiteral("nine"), QStringLiteral("9")},
        {QStringLiteral("ten"), QStringLiteral("10")},
        {QStringLiteral("eleven"), QStringLiteral("11")},
        {QStringLiteral("twelve"), QStringLiteral("12")}
    };

    for (QString &word : words) {
        const auto digits = numbers.constFind(word);
        if (digits != numbers.constEnd()) {
            word = digits.value();
        }
    }
    return words;
}

QString comparable(const QString &title)
{
    return withArabicNumerals(withNumberWords(wordsOf(title)));
}

bool namesBothOf(const QString &parsedTitle, const QString &title,
                 const QString &originalTitle)
{
    const QString parsed = comparable(parsedTitle);
    const QString english = comparable(title);
    const QString original = comparable(originalTitle);
    if (parsed.isEmpty() || english.isEmpty() || original.isEmpty()
        || english == original) {
        return false;
    }
    return parsed == english + QLatin1Char(' ') + original
        || parsed == original + QLatin1Char(' ') + english;
}

}

QString normaliseTitle(const QString &title)
{
    return withArabicNumerals(wordsOf(title));
}

int titleSimilarity(const QString &parsedTitle, const QString &candidateTitle)
{
    const QStringList leftWords = wordsOf(parsedTitle);
    const QStringList rightWords = wordsOf(candidateTitle);
    const QString left = withArabicNumerals(withNumberWords(leftWords));
    const QString right = withArabicNumerals(withNumberWords(rightWords));

    if (left.isEmpty() || right.isEmpty()) {
        return 0;
    }

    if (left == right) {
        return 100;
    }

    if (left.size() < 2 || right.size() < 2
        || spelledOutLength(leftWords) < 2 || spelledOutLength(rightWords) < 2) {
        return 0;
    }

    const QString leftPadded = left + QLatin1Char(' ');
    const QString rightPadded = right + QLatin1Char(' ');
    if (leftPadded.startsWith(rightPadded) || rightPadded.startsWith(leftPadded)
        || left.endsWith(QLatin1Char(' ') + right)
        || right.endsWith(QLatin1Char(' ') + left)) {
        return 78;
    }

    const QString leftSpaced = QLatin1Char(' ') + left + QLatin1Char(' ');
    const QString rightSpaced = QLatin1Char(' ') + right + QLatin1Char(' ');
    if (leftSpaced.contains(rightSpaced) || rightSpaced.contains(leftSpaced)) {
        return 66;
    }

    const QSet<QString> leftTokens = tokensOf(left);
    const QSet<QString> rightTokens = tokensOf(right);
    if (leftTokens.isEmpty() || rightTokens.isEmpty()) {
        return 0;
    }

    const QSet<QString> shared = leftTokens & rightTokens;
    const int union_ = leftTokens.size() + rightTokens.size() - shared.size();
    if (union_ <= 0) {
        return 0;
    }

    return (shared.size() * 100) / union_;
}

MatchScore score(const ParsedFileName &parsed, const TmdbTitleResultDto &candidate)
{
    MatchScore result;
    QStringList notes;

    if (isGenericStem(normaliseTitle(parsed.title), parsed.year)) {
        result.value = 0;
        result.reason = QStringLiteral("the filename describes the file, "
                                       "not a title");
        return result;
    }

    const QString candidateTitle = QString::fromStdString(candidate.title);
    const QString originalTitle = QString::fromStdString(candidate.originalTitle);

    const int englishSimilarity = titleSimilarity(parsed.title, candidateTitle);
    const int originalSimilarity = originalTitle.isEmpty()
        ? 0
        : titleSimilarity(parsed.title, originalTitle);
    const bool bothNames = namesBothOf(parsed.title, candidateTitle, originalTitle);
    const int similarity = bothNames ? 100 : qMax(englishSimilarity, originalSimilarity);

    double value = similarity * 0.75;
    if (bothNames) {
        notes.append(QStringLiteral("title 100 on both of its names"));
    } else if (originalSimilarity > englishSimilarity) {
        notes.append(QStringLiteral("title %1 on the original name")
                         .arg(similarity));
    } else {
        notes.append(QStringLiteral("title %1").arg(similarity));
    }

    const bool expectedTv = parsed.looksLikeEpisode();
    const bool candidateIsTv = candidate.mediaType == TmdbMediaType::Tv;

    if (expectedTv == candidateIsTv) {
        value += 12.0;
        notes.append(expectedTv ? QStringLiteral("tv +12")
                                : QStringLiteral("movie +12"));
    } else {
        value -= 30.0;
        notes.append(QStringLiteral("wrong media type -30"));
    }

    const int candidateYear = releaseYearOf(candidate.releaseDate);
    bool yearAgrees = false;

    if (parsed.year > 0 && candidateYear > 0) {
        const int distance = qAbs(parsed.year - candidateYear);
        yearAgrees = distance <= 1;
        if (distance == 0) {
            value += 15.0;
            notes.append(QStringLiteral("year exact +15"));
        } else if (distance == 1) {
            value += 6.0;
            notes.append(QStringLiteral("year off by one +6"));
        } else {
            value -= qMin(25, distance * 6);
            notes.append(QStringLiteral("year off by %1 -%2")
                             .arg(distance)
                             .arg(qMin(25, distance * 6)));
        }
    } else if (parsed.year > 0 && candidateYear == 0) {
        notes.append(QStringLiteral("candidate has no year"));
    }

    if (candidate.popularity > 0.0) {
        const double bump = qMin(3.0, candidate.popularity / 100.0);
        value += bump;
    }

    result.value = qBound(0, static_cast<int>(value + 0.5), 100);

    const bool exactTitle = similarity == 100;
    const bool namelessYear = parsed.year == 0;
    const bool corroborated = yearAgrees
        || (expectedTv && candidateIsTv)
        || (exactTitle && namelessYear);

    if (exactTitle && namelessYear) {
        notes.append(QStringLiteral("exact title, no year in the name"));
    }

    if (!corroborated && result.value >= 80) {
        result.value = 79;
        notes.append(QStringLiteral("capped, nothing corroborates the title"));
    }

    result.reason = notes.join(QStringLiteral(", "));
    return result;
}

}
