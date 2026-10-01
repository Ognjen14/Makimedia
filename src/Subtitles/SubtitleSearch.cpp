#include "Subtitles/SubtitleSearch.h"

#include "AppSettings.h"
#include "Config/ApiKeys.h"
#include "Library/SubtitleNaming.h"
#include "MmLog.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include <algorithm>

namespace {

constexpr int kHashChunk = 64 * 1024;
constexpr qint64 kMaxSubtitleBytes = 4 * 1024 * 1024;

QByteArray apiKey()
{
    using namespace Makimedia::Config;
    if (OpenSubtitlesApiKeyLength == 0) {
        return QByteArray();
    }
    QByteArray key;
    key.resize(int(OpenSubtitlesApiKeyLength));
    for (std::size_t i = 0; i < OpenSubtitlesApiKeyLength; ++i) {
        key[int(i)] = char(OpenSubtitlesApiKeyObfuscated[i]
                           ^ OpenSubtitlesApiKeyMask[i % sizeof(OpenSubtitlesApiKeyMask)]);
    }
    return key;
}

QString userAgent()
{
    return QStringLiteral("Makimedia v%1").arg(QCoreApplication::applicationVersion());
}

QString reasonFor(QNetworkReply *reply)
{
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QByteArray body = reply->readAll();
    if (!body.isEmpty()) {
        const QJsonObject said = QJsonDocument::fromJson(body).object();
        const QString message = said.value(QStringLiteral("message")).toString();
        MM_LOG_W() << "opensubtitles answered" << status
                   << "for" << reply->url().path() << "-"
                   << (message.isEmpty() ? QString::fromUtf8(body.left(400))
                                         : message);
        if (!message.isEmpty() && status == 400) {
            return message;
        }
    } else {
        MM_LOG_W() << "opensubtitles answered" << status << "for"
                   << reply->url().path() << "with nothing to say";
    }

    switch (status) {
    case 401:
        return SubtitleSearch::tr("Your opensubtitles account was not accepted.");
    case 403:
        return SubtitleSearch::tr("This copy of Makimedia was refused by opensubtitles.");
    case 406:
        return SubtitleSearch::tr("No downloads left today.");
    case 429:
        return SubtitleSearch::tr("Too many requests just now. Try again in a minute.");
    default:
        break;
    }
    if (status >= 500) {
        return SubtitleSearch::tr("opensubtitles is not answering.");
    }
    return reply->errorString();
}

}

SubtitleSearch::SubtitleSearch(AppSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_host(QStringLiteral("api.opensubtitles.com"))
{
    if (!available()) {
        MM_LOG_I() << "subtitle search is unavailable on this build";
        return;
    }

    m_network = new QNetworkAccessManager(this);
    MM_LOG_I() << "subtitle search ready, key is" << apiKey().size()
               << "characters, identifying as" << userAgent();

    if (!m_settings.subtitleAccount().isEmpty()
        && !m_settings.subtitlePassword().isEmpty()) {
        QTimer::singleShot(8000, this, [this]() {
            if (!signedIn() && !m_busy) {
                signIn();
            }
        });
    }
}

SubtitleSearch::~SubtitleSearch() = default;

bool SubtitleSearch::available() const
{
    return Makimedia::Config::OpenSubtitlesApiKeyLength > 0;
}

bool SubtitleSearch::signedIn() const
{
    return !m_token.isEmpty();
}

bool SubtitleSearch::busy() const
{
    return m_busy;
}

int SubtitleSearch::downloadsLeft() const
{
    return m_left;
}

QVariantList SubtitleSearch::results() const
{
    QVariantList list;
    list.reserve(m_candidates.size());
    for (const Candidate &candidate : m_candidates) {
        QVariantMap entry;
        entry.insert(QStringLiteral("release"), candidate.release);
        entry.insert(QStringLiteral("language"), candidate.language);
        entry.insert(QStringLiteral("name"), candidate.name);
        entry.insert(QStringLiteral("hashMatched"), candidate.hashMatched);
        entry.insert(QStringLiteral("downloads"), candidate.downloads);
        list.append(entry);
    }
    return list;
}

QString SubtitleSearch::movieHash(const QString &videoPath)
{
    QFile file(videoPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    const qint64 size = file.size();
    if (size < kHashChunk * 2) {
        return QString();
    }

    quint64 hash = quint64(size);
    const auto consume = [&hash](QDataStream &stream) {
        quint64 word = 0;
        for (int i = 0; i < kHashChunk / int(sizeof(quint64)); ++i) {
            stream >> word;
            hash += word;
        }
    };

    QDataStream head(&file);
    head.setByteOrder(QDataStream::LittleEndian);
    consume(head);

    if (!file.seek(size - kHashChunk)) {
        return QString();
    }
    QDataStream tail(&file);
    tail.setByteOrder(QDataStream::LittleEndian);
    consume(tail);

    return QStringLiteral("%1").arg(hash, 16, 16, QLatin1Char('0'));
}

void SubtitleSearch::find(const QString &videoPath, qint64 tmdbId,
                          int season, int episode)
{
    if (!available() || m_busy) {
        return;
    }

    m_videoPath = videoPath;
    m_tmdbId = tmdbId;
    m_season = season;
    m_episode = episode;

    forget();

    if (m_settings.subtitleAccount().isEmpty()
        || m_settings.subtitlePassword().isEmpty()) {
        emit failed(tr("Add your opensubtitles account in Settings first."));
        return;
    }

    if (signedIn()) {
        setBusy(true);
        runSearch();
    } else {
        m_searchWhenSignedIn = true;
        signIn();
    }
}

void SubtitleSearch::forget()
{
    if (m_candidates.isEmpty()) {
        return;
    }
    m_candidates.clear();
    emit resultsChanged();
}

void SubtitleSearch::signIn()
{
    if (!available() || m_busy) {
        return;
    }

    if (m_settings.subtitleAccount().isEmpty()
        || m_settings.subtitlePassword().isEmpty()) {
        emit failed(tr("Enter your opensubtitles username and password first."));
        return;
    }

    if (!m_token.isEmpty()) {
        m_token.clear();
        emit signedInChanged();
    }
    setBusy(true);

    MM_LOG_I() << "signing in to opensubtitles as"
               << m_settings.subtitleAccount();

    const QJsonObject body{
        { QStringLiteral("username"), m_settings.subtitleAccount() },
        { QStringLiteral("password"), m_settings.subtitlePassword() }
    };

    QNetworkReply *reply = post(QStringLiteral("/api/v1/login"),
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (!reply) {
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_searchWhenSignedIn = false;
            stop(reasonFor(reply));
            return;
        }

        const QJsonObject answer =
            QJsonDocument::fromJson(reply->readAll()).object();
        m_token = answer.value(QStringLiteral("token")).toString();
        const QString base = answer.value(QStringLiteral("base_url")).toString();
        if (!base.isEmpty()) {
            m_host = base;
        }
        if (m_token.isEmpty()) {
            m_searchWhenSignedIn = false;
            stop(tr("Your opensubtitles account was not accepted."));
            return;
        }

        MM_LOG_I() << "signed in to opensubtitles as"
                   << m_settings.subtitleAccount() << "through" << m_host;
        emit signedInChanged();

        askQuota();

        if (m_searchWhenSignedIn) {
            m_searchWhenSignedIn = false;
            runSearch();
        } else {
            setBusy(false);
        }
    });
}

void SubtitleSearch::askQuota()
{
    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(m_host);
    url.setPath(QStringLiteral("/api/v1/infos/user"));

    QNetworkReply *reply = get(url.toString());
    if (!reply) {
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            MM_LOG_W() << "could not read the opensubtitles quota"
                       << reply->errorString();
            return;
        }

        const QJsonObject user = QJsonDocument::fromJson(reply->readAll())
                                     .object()
                                     .value(QStringLiteral("data"))
                                     .toObject();
        if (!user.contains(QStringLiteral("remaining_downloads"))) {
            return;
        }

        m_left = user.value(QStringLiteral("remaining_downloads")).toInt();
        MM_LOG_I() << "opensubtitles quota:" << m_left << "left of"
                   << user.value(QStringLiteral("allowed_downloads")).toInt()
                   << "today";
        emit quotaChanged();
    });
}

void SubtitleSearch::runSearch()
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("languages"),
                       m_settings.subtitleSearchLanguages().join(QLatin1Char(',')));

    if (m_tmdbId > 0) {
        if (m_episode > 0) {
            query.addQueryItem(QStringLiteral("parent_tmdb_id"),
                               QString::number(m_tmdbId));
            query.addQueryItem(QStringLiteral("season_number"),
                               QString::number(m_season));
            query.addQueryItem(QStringLiteral("episode_number"),
                               QString::number(m_episode));
        } else {
            query.addQueryItem(QStringLiteral("tmdb_id"), QString::number(m_tmdbId));
        }
    }

    const QString hash = movieHash(m_videoPath);
    if (!hash.isEmpty()) {
        query.addQueryItem(QStringLiteral("moviehash"), hash);
    }

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(m_host);
    url.setPath(QStringLiteral("/api/v1/subtitles"));
    url.setQuery(query);

    MM_LOG_I() << "asking opensubtitles for" << QFileInfo(m_videoPath).fileName()
               << "in" << m_settings.subtitleSearchLanguages()
               << (hash.isEmpty() ? "without a hash" : "with its hash");

    QNetworkReply *reply = get(url.toString());
    if (!reply) {
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            stop(reasonFor(reply));
            return;
        }
        takeCandidates(reply->readAll());
    });
}

void SubtitleSearch::takeCandidates(const QByteArray &body)
{
    const QJsonArray data = QJsonDocument::fromJson(body)
                                .object()
                                .value(QStringLiteral("data"))
                                .toArray();

    for (const QJsonValue &value : data) {
        const QJsonObject attributes =
            value.toObject().value(QStringLiteral("attributes")).toObject();
        const QJsonArray files =
            attributes.value(QStringLiteral("files")).toArray();
        if (files.isEmpty()) {
            continue;
        }

        const QJsonObject file = files.first().toObject();
        Candidate candidate;
        candidate.fileId = file.value(QStringLiteral("file_id")).toVariant().toLongLong();
        candidate.name = file.value(QStringLiteral("file_name")).toString();
        candidate.release = attributes.value(QStringLiteral("release")).toString();
        candidate.language = attributes.value(QStringLiteral("language")).toString();
        candidate.hashMatched =
            attributes.value(QStringLiteral("moviehash_match")).toBool();
        candidate.downloads =
            attributes.value(QStringLiteral("download_count")).toInt();

        if (candidate.fileId > 0) {
            m_candidates.append(candidate);
        }
    }

    std::stable_sort(m_candidates.begin(), m_candidates.end(),
                     [](const Candidate &a, const Candidate &b) {
        if (a.hashMatched != b.hashMatched) {
            return a.hashMatched;
        }
        return a.downloads > b.downloads;
    });

    MM_LOG_I() << "opensubtitles offered" << m_candidates.size() << "subtitles,"
               << std::count_if(m_candidates.cbegin(), m_candidates.cend(),
                                [](const Candidate &c) { return c.hashMatched; })
               << "matching this exact file";

    setBusy(false);
    emit resultsChanged();

    if (m_candidates.isEmpty()) {
        emit failed(tr("Nothing found for this one."));
    }
}

void SubtitleSearch::fetch(int index)
{
    if (!available() || m_busy || index < 0 || index >= m_candidates.size()) {
        return;
    }
    setBusy(true);
    requestLink(m_candidates.at(index));
}

void SubtitleSearch::requestLink(const Candidate &candidate)
{
    const QJsonObject body{
        { QStringLiteral("file_id"), double(candidate.fileId) }
    };

    QNetworkReply *reply = post(QStringLiteral("/api/v1/download"),
                                QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (!reply) {
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, candidate]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            stop(reasonFor(reply));
            return;
        }

        const QJsonObject answer =
            QJsonDocument::fromJson(reply->readAll()).object();
        if (answer.contains(QStringLiteral("remaining"))) {
            m_left = answer.value(QStringLiteral("remaining")).toInt();
            emit quotaChanged();
        }

        const QString link = answer.value(QStringLiteral("link")).toString();
        if (link.isEmpty()) {
            stop(tr("opensubtitles gave no download link."));
            return;
        }
        collect(link, candidate);
    });
}

void SubtitleSearch::collect(const QString &url, const Candidate &candidate)
{
    QNetworkReply *reply = get(url);
    if (!reply) {
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, candidate]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            stop(reasonFor(reply));
            return;
        }

        const QByteArray bytes = reply->readAll();
        if (bytes.isEmpty() || bytes.size() > kMaxSubtitleBytes) {
            stop(tr("That subtitle did not arrive in one piece."));
            return;
        }
        store(bytes, candidate);
    });
}

void SubtitleSearch::store(const QByteArray &bytes, const Candidate &candidate)
{
    const QFileInfo video(m_videoPath);
    const QString base = video.completeBaseName();
    const QString language = candidate.language.isEmpty()
                                 ? QStringLiteral("srt")
                                 : candidate.language.toLower();

    QString suffix = QFileInfo(candidate.name).suffix().toLower();
    if (!SubtitleNaming::subtitleSuffixes().contains(suffix)) {
        suffix = QStringLiteral("srt");
    }

    const auto nameIn = [&](const QString &dir, int n) {
        const QString stem = n < 2
            ? base + QLatin1Char('.') + language
            : base + QLatin1Char('.') + language + QLatin1Char('.') + QString::number(n);
        return dir + QLatin1Char('/') + stem + QLatin1Char('.') + suffix;
    };

    const auto writeInto = [&](const QString &dir) {
        if (dir.isEmpty()) {
            return QString();
        }
        QString path = nameIn(dir, 1);
        for (int n = 2; QFile::exists(path) && n < 100; ++n) {
            path = nameIn(dir, n);
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || file.write(bytes) != bytes.size()) {
            MM_LOG_W() << "could not write the subtitle to" << path << file.errorString();
            return QString();
        }
        file.close();
        return path;
    };

    QString target = writeInto(video.absolutePath());
    if (target.isEmpty()) {
        const QString kept = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + QStringLiteral("/subtitles");
        if (QDir().mkpath(kept)) {
            MM_LOG_I() << "the film's own folder would not take the subtitle, "
                          "keeping it with the app instead";
            target = writeInto(kept);
        }
    }

    if (target.isEmpty()) {
        stop(tr("The subtitle could not be saved."));
        return;
    }

    MM_LOG_I() << "saved" << candidate.language << "subtitle to" << target
               << "-" << m_left << "downloads left today";

    setBusy(false);
    emit saved(target, QFileInfo(target).fileName());
}

void SubtitleSearch::stop(const QString &reason)
{
    setBusy(false);
    MM_LOG_W() << "subtitle search stopped:" << reason;
    emit failed(reason);
}

void SubtitleSearch::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

QNetworkReply *SubtitleSearch::post(const QString &path, const QByteArray &body)
{
    if (!m_network) {
        return nullptr;
    }

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(m_host);
    url.setPath(path);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Api-Key", apiKey());
    request.setRawHeader("User-Agent", userAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    }
    return m_network->post(request, body);
}

QNetworkReply *SubtitleSearch::get(const QString &url)
{
    if (!m_network) {
        return nullptr;
    }

    QNetworkRequest request{ QUrl(url) };
    request.setRawHeader("Api-Key", apiKey());
    request.setRawHeader("User-Agent", userAgent().toUtf8());
    request.setRawHeader("Accept", "application/json");
    if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return m_network->get(request);
}
