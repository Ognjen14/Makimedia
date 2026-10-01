#include "Library/SubtitleNaming.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace {

QStringList tokensOf(const QString &baseName)
{
    static const QRegularExpression separators(QStringLiteral("[._\\-\\s]"));
    return baseName.split(separators, Qt::SkipEmptyParts);
}

bool isDescriptor(const QString &token)
{
    static const QSet<QString> descriptors = {
        QStringLiteral("forced"), QStringLiteral("sdh"), QStringLiteral("hi"),
        QStringLiteral("cc"), QStringLiteral("default"), QStringLiteral("full")
    };
    return descriptors.contains(token.toLower());
}

bool hasMixedCase(const QString &token)
{
    return token != token.toLower() && token != token.toUpper();
}

QString leafOf(const QString &pathOrName)
{
    const QString trimmed = QUrl::fromPercentEncoding(pathOrName.trimmed().toUtf8());

    const int slash = qMax(trimmed.lastIndexOf(QLatin1Char('/')),
                           trimmed.lastIndexOf(QLatin1Char('\\')));
    QString leaf = slash >= 0 ? trimmed.mid(slash + 1) : trimmed;

    const int colon = leaf.lastIndexOf(QLatin1Char(':'));
    if (colon >= 0) {
        leaf = leaf.mid(colon + 1);
    }
    return leaf;
}

QString typeOf(const QString &pathOrName)
{
    const QString leaf = leafOf(pathOrName);
    const int dot = leaf.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot + 1 >= leaf.size()) {
        return QString();
    }
    return leaf.mid(dot + 1).toUpper();
}

}

namespace SubtitleNaming {

QString baseName(const QString &pathOrName)
{
    const QString leaf = leafOf(pathOrName);
    const int dot = leaf.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? leaf.left(dot) : leaf;
}

const QStringList &subtitleSuffixes()
{
    static const QStringList suffixes = {
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("sub"), QStringLiteral("vtt"), QStringLiteral("idx")
    };
    return suffixes;
}

bool isSubtitleFile(const QString &pathOrName)
{
    const QString leaf = leafOf(pathOrName);
    const int dot = leaf.lastIndexOf(QLatin1Char('.'));
    return dot > 0 && subtitleSuffixes().contains(leaf.mid(dot + 1).toLower());
}

bool belongsToVideo(const QString &videoPathOrName, const QString &subtitlePathOrName)
{
    if (!isSubtitleFile(subtitlePathOrName)) {
        return false;
    }

    const QString video = baseName(videoPathOrName);
    const QString subtitle = baseName(subtitlePathOrName);
    if (video.isEmpty() || !subtitle.startsWith(video, Qt::CaseInsensitive)) {
        return false;
    }
    return subtitle.size() == video.size()
        || !subtitle.at(video.size()).isLetterOrNumber();
}

bool belongsToVideo(const QString &videoPathOrName, const QString &subtitlePathOrName,
                    Place place, bool onlyVideoInFolder)
{
    if (!isSubtitleFile(subtitlePathOrName)) {
        return false;
    }

    switch (place) {
    case Place::FolderNamedAfterVideo:
        return true;
    case Place::SubtitleFolder:
        if (onlyVideoInFolder) {
            return true;
        }
        break;
    case Place::BesideVideo:
        break;
    }

    return belongsToVideo(videoPathOrName, subtitlePathOrName);
}

static const QHash<QString, QString> &languageTable()
{
    static const QHash<QString, QString> languages = {
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("eng"), QStringLiteral("English")},
        {QStringLiteral("english"), QStringLiteral("English")},
        {QStringLiteral("hr"), QStringLiteral("Croatian")},
        {QStringLiteral("hrv"), QStringLiteral("Croatian")},
        {QStringLiteral("croatian"), QStringLiteral("Croatian")},
        {QStringLiteral("sr"), QStringLiteral("Serbian")},
        {QStringLiteral("srp"), QStringLiteral("Serbian")},
        {QStringLiteral("bs"), QStringLiteral("Bosnian")},
        {QStringLiteral("sl"), QStringLiteral("Slovenian")},
        {QStringLiteral("de"), QStringLiteral("German")},
        {QStringLiteral("ger"), QStringLiteral("German")},
        {QStringLiteral("deu"), QStringLiteral("German")},
        {QStringLiteral("german"), QStringLiteral("German")},
        {QStringLiteral("fr"), QStringLiteral("French")},
        {QStringLiteral("fre"), QStringLiteral("French")},
        {QStringLiteral("fra"), QStringLiteral("French")},
        {QStringLiteral("french"), QStringLiteral("French")},
        {QStringLiteral("es"), QStringLiteral("Spanish")},
        {QStringLiteral("spa"), QStringLiteral("Spanish")},
        {QStringLiteral("spanish"), QStringLiteral("Spanish")},
        {QStringLiteral("it"), QStringLiteral("Italian")},
        {QStringLiteral("ita"), QStringLiteral("Italian")},
        {QStringLiteral("pt"), QStringLiteral("Portuguese")},
        {QStringLiteral("por"), QStringLiteral("Portuguese")},
        {QStringLiteral("ru"), QStringLiteral("Russian")},
        {QStringLiteral("rus"), QStringLiteral("Russian")},
        {QStringLiteral("ja"), QStringLiteral("Japanese")},
        {QStringLiteral("jpn"), QStringLiteral("Japanese")},
        {QStringLiteral("ko"), QStringLiteral("Korean")},
        {QStringLiteral("kor"), QStringLiteral("Korean")},
        {QStringLiteral("zh"), QStringLiteral("Chinese")},
        {QStringLiteral("chi"), QStringLiteral("Chinese")},
        {QStringLiteral("zho"), QStringLiteral("Chinese")},
        {QStringLiteral("nl"), QStringLiteral("Dutch")},
        {QStringLiteral("dut"), QStringLiteral("Dutch")},
        {QStringLiteral("pl"), QStringLiteral("Polish")},
        {QStringLiteral("pol"), QStringLiteral("Polish")},
        {QStringLiteral("sv"), QStringLiteral("Swedish")},
        {QStringLiteral("swe"), QStringLiteral("Swedish")},
        {QStringLiteral("da"), QStringLiteral("Danish")},
        {QStringLiteral("no"), QStringLiteral("Norwegian")},
        {QStringLiteral("nor"), QStringLiteral("Norwegian")},
        {QStringLiteral("fi"), QStringLiteral("Finnish")},
        {QStringLiteral("tr"), QStringLiteral("Turkish")},
        {QStringLiteral("ar"), QStringLiteral("Arabic")},
        {QStringLiteral("cs"), QStringLiteral("Czech")},
        {QStringLiteral("cze"), QStringLiteral("Czech")},
        {QStringLiteral("hu"), QStringLiteral("Hungarian")},
        {QStringLiteral("el"), QStringLiteral("Greek")},
        {QStringLiteral("he"), QStringLiteral("Hebrew")},
        {QStringLiteral("hin"), QStringLiteral("Hindi")},
        {QStringLiteral("hindi"), QStringLiteral("Hindi")},
        {QStringLiteral("th"), QStringLiteral("Thai")},
        {QStringLiteral("vi"), QStringLiteral("Vietnamese")},
        {QStringLiteral("uk"), QStringLiteral("Ukrainian")},
        {QStringLiteral("ro"), QStringLiteral("Romanian")},
        {QStringLiteral("bg"), QStringLiteral("Bulgarian")},
        {QStringLiteral("sk"), QStringLiteral("Slovak")},
        {QStringLiteral("ind"), QStringLiteral("Indonesian")},
        {QStringLiteral("ara"), QStringLiteral("Arabic")},
        {QStringLiteral("dan"), QStringLiteral("Danish")},
        {QStringLiteral("fin"), QStringLiteral("Finnish")},
        {QStringLiteral("gre"), QStringLiteral("Greek")},
        {QStringLiteral("ell"), QStringLiteral("Greek")},
        {QStringLiteral("heb"), QStringLiteral("Hebrew")},
        {QStringLiteral("hun"), QStringLiteral("Hungarian")},
        {QStringLiteral("tur"), QStringLiteral("Turkish")},
        {QStringLiteral("ukr"), QStringLiteral("Ukrainian")},
        {QStringLiteral("vie"), QStringLiteral("Vietnamese")},
        {QStringLiteral("tha"), QStringLiteral("Thai")},
        {QStringLiteral("rum"), QStringLiteral("Romanian")},
        {QStringLiteral("ron"), QStringLiteral("Romanian")},
        {QStringLiteral("bul"), QStringLiteral("Bulgarian")},
        {QStringLiteral("slk"), QStringLiteral("Slovak")},
        {QStringLiteral("slo"), QStringLiteral("Slovak")},
        {QStringLiteral("slv"), QStringLiteral("Slovenian")},
        {QStringLiteral("ces"), QStringLiteral("Czech")},
        {QStringLiteral("nob"), QStringLiteral("Norwegian")},
        {QStringLiteral("nld"), QStringLiteral("Dutch")},
        {QStringLiteral("et"), QStringLiteral("Estonian")},
        {QStringLiteral("est"), QStringLiteral("Estonian")},
        {QStringLiteral("lv"), QStringLiteral("Latvian")},
        {QStringLiteral("lav"), QStringLiteral("Latvian")},
        {QStringLiteral("lt"), QStringLiteral("Lithuanian")},
        {QStringLiteral("lit"), QStringLiteral("Lithuanian")}
    };
    return languages;
}

QString languageOf(const QString &baseName)
{
    const QHash<QString, QString> &languages = languageTable();

    QStringList tokens = tokensOf(baseName);
    while (!tokens.isEmpty() && isDescriptor(tokens.last())) {
        tokens.removeLast();
    }
    if (tokens.size() < 2) {
        return QString();
    }

    const QString &token = tokens.last();
    const auto found = languages.constFind(token.toLower());
    if (found == languages.constEnd()) {
        return QString();
    }
    if (token.size() == 2 && hasMixedCase(token)) {
        return QString();
    }
    return found.value();
}

static const QHash<QString, QString> &codeTable()
{
    static const QHash<QString, QString> codes = {
        {QStringLiteral("English"), QStringLiteral("en")},
        {QStringLiteral("Croatian"), QStringLiteral("hr")},
        {QStringLiteral("Serbian"), QStringLiteral("sr")},
        {QStringLiteral("Bosnian"), QStringLiteral("bs")},
        {QStringLiteral("Slovenian"), QStringLiteral("sl")},
        {QStringLiteral("German"), QStringLiteral("de")},
        {QStringLiteral("French"), QStringLiteral("fr")},
        {QStringLiteral("Spanish"), QStringLiteral("es")},
        {QStringLiteral("Italian"), QStringLiteral("it")},
        {QStringLiteral("Portuguese"), QStringLiteral("pt")},
        {QStringLiteral("Russian"), QStringLiteral("ru")},
        {QStringLiteral("Japanese"), QStringLiteral("ja")},
        {QStringLiteral("Korean"), QStringLiteral("ko")},
        {QStringLiteral("Chinese"), QStringLiteral("zh")},
        {QStringLiteral("Dutch"), QStringLiteral("nl")},
        {QStringLiteral("Polish"), QStringLiteral("pl")},
        {QStringLiteral("Swedish"), QStringLiteral("sv")},
        {QStringLiteral("Danish"), QStringLiteral("da")},
        {QStringLiteral("Norwegian"), QStringLiteral("no")},
        {QStringLiteral("Finnish"), QStringLiteral("fi")},
        {QStringLiteral("Turkish"), QStringLiteral("tr")},
        {QStringLiteral("Arabic"), QStringLiteral("ar")},
        {QStringLiteral("Czech"), QStringLiteral("cs")},
        {QStringLiteral("Hungarian"), QStringLiteral("hu")},
        {QStringLiteral("Greek"), QStringLiteral("el")},
        {QStringLiteral("Hebrew"), QStringLiteral("he")},
        {QStringLiteral("Hindi"), QStringLiteral("hi")},
        {QStringLiteral("Thai"), QStringLiteral("th")},
        {QStringLiteral("Vietnamese"), QStringLiteral("vi")},
        {QStringLiteral("Ukrainian"), QStringLiteral("uk")},
        {QStringLiteral("Romanian"), QStringLiteral("ro")},
        {QStringLiteral("Bulgarian"), QStringLiteral("bg")},
        {QStringLiteral("Slovak"), QStringLiteral("sk")},
        {QStringLiteral("Indonesian"), QStringLiteral("id")},
        {QStringLiteral("Estonian"), QStringLiteral("et")},
        {QStringLiteral("Latvian"), QStringLiteral("lv")},
        {QStringLiteral("Lithuanian"), QStringLiteral("lt")}
    };
    return codes;
}

Description describe(const QString &videoPathOrName, const QString &subtitlePathOrName)
{
    const QString video = baseName(videoPathOrName);
    const QString subtitle = baseName(subtitlePathOrName);
    const bool namedAfterVideo =
        !video.isEmpty() && subtitle.startsWith(video, Qt::CaseInsensitive);
    const QString rest = namedAfterVideo ? subtitle.mid(video.size()) : subtitle;

    const QStringList tokens = tokensOf(rest);
    Description result;

    qsizetype languageAt = -1;
    for (qsizetype i = tokens.size() - 1; i >= 0; --i) {
        const QString &token = tokens.at(i);
        if (isDescriptor(token)) {
            continue;
        }
        const auto found = languageTable().constFind(token.toLower());
        if (found != languageTable().constEnd()
            && !(token.size() == 2 && hasMixedCase(token))) {
            result.language = found.value();
            languageAt = i;
        }
        break;
    }

    if (result.language.isEmpty()) {
        const QString type = typeOf(subtitlePathOrName);
        if (namedAfterVideo && !type.isEmpty()) {
            result.title = tokens.isEmpty()
                ? type
                : type + QLatin1Char(' ') + tokens.join(QLatin1Char(' '));
        } else if (!tokens.isEmpty()) {
            result.title = tokens.join(QLatin1Char(' '));
        } else if (!type.isEmpty()) {
            result.title = type;
        } else {
            result.title = subtitle.isEmpty() ? QObject::tr("External subtitle") : subtitle;
        }
        return result;
    }
    result.code = codeTable().value(result.language);

    QStringList qualifiers;
    QStringList notes;
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        if (i == languageAt) {
            continue;
        }
        const QString &token = tokens.at(i);
        const QString tag = token.toLower();
        if (tag == QLatin1String("forced")) {
            const QString note = QObject::tr("forced");
            if (!notes.contains(note)) {
                notes.append(note);
            }
        } else if (tag == QLatin1String("sdh") || tag == QLatin1String("hi")
                   || tag == QLatin1String("cc")) {
            if (!notes.contains(QStringLiteral("SDH"))) {
                notes.append(QStringLiteral("SDH"));
            }
        } else if (!isDescriptor(token)
                   && std::all_of(token.cbegin(), token.cend(),
                                  [](QChar c) { return c.isLetter(); })) {
            qualifiers.append(token);
        }
    }

    QStringList parts;
    if (!qualifiers.isEmpty()) {
        parts.append(qualifiers.join(QLatin1Char(' ')));
    }
    parts.append(notes);

    result.title = parts.isEmpty()
        ? result.language
        : QStringLiteral("%1 (%2)").arg(result.language, parts.join(QStringLiteral(", ")));
    return result;
}

QString titleFor(const QString &baseName, const QString &language)
{
    if (language.isEmpty()) {
        return baseName.isEmpty() ? QObject::tr("External subtitle") : baseName;
    }

    QStringList notes;
    const QStringList tokens = tokensOf(baseName);
    for (qsizetype i = tokens.size() - 1; i >= 0 && isDescriptor(tokens.at(i)); --i) {
        const QString tag = tokens.at(i).toLower();
        QString note;
        if (tag == QLatin1String("forced")) {
            note = QObject::tr("forced");
        } else if (tag == QLatin1String("sdh") || tag == QLatin1String("hi")
                   || tag == QLatin1String("cc")) {
            note = QStringLiteral("SDH");
        }
        if (!note.isEmpty() && !notes.contains(note)) {
            notes.prepend(note);
        }
    }

    return notes.isEmpty()
        ? language
        : QStringLiteral("%1 (%2)").arg(language, notes.join(QStringLiteral(", ")));
}

}
