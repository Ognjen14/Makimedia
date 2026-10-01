#include "Library/ShowEpisodeModel.h"

#include "Data/PlaybackStateRepository.h"
#include "MmLog.h"
#include "Library/ListPatch.h"

#include <QElapsedTimer>

#include <algorithm>
#include <utility>

namespace {

double progressOf(const EpisodeRecord &episode)
{
    const double duration = episode.durationSeconds;
    return duration > 0 ? qBound(0.0, episode.positionSeconds / duration, 1.0) : 0.0;
}

double resumeOf(const EpisodeRecord &episode)
{
    return PlaybackStateRepository::resumeSeconds(episode.positionSeconds,
                                                  episode.durationSeconds,
                                                  episode.watched);
}

}

ShowEpisodeModel::ShowEpisodeModel(const MediaRepository &media, QObject *parent)
    : QAbstractListModel(parent)
    , m_media(media)
{
}

int ShowEpisodeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant ShowEpisodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const EpisodeRecord &episode = m_rows.at(index.row());

    switch (role) {
    case SeasonRole:
        return episode.season;
    case EpisodeRole:
        return episode.episode;
    case TitleRole:
        return episode.title;
    case OverviewRole:
        return episode.overview;
    case AirDateRole:
        return episode.airDate;
    case StillPathRole:
        return episode.stillPath;
    case RuntimeMinutesRole:
        return episode.runtimeMinutes;
    case HandleRole:
        return episode.fileHandle;
    case FileNameRole:
        return episode.fileName;
    case HasFileRole:
        return episode.hasFile();
    case MissingRole:
        return episode.missing;
    case WatchedRole:
        return episode.watched;
    case PositionSecondsRole:
        return episode.positionSeconds;
    case DurationSecondsRole:
        return episode.durationSeconds;
    case ProgressRole:
        return progressOf(episode);
    case ResumeSecondsRole:
        return resumeOf(episode);
    case ArtworkStampRole:
        return m_artworkEverything + m_artworkStamps.value(episode.stillPath);
    default:
        return QVariant();
    }
}

void ShowEpisodeModel::touchArtwork(const QStringList &paths, bool everything)
{
    if (everything) {
        ++m_artworkEverything;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0, 0), index(int(m_rows.size()) - 1, 0),
                             {ArtworkStampRole});
        }
        return;
    }

    if (paths.isEmpty()) {
        return;
    }

    const QSet<QString> landed(paths.cbegin(), paths.cend());
    for (const QString &path : landed) {
        ++m_artworkStamps[path];
    }

    for (int row = 0; row < m_rows.size(); ++row) {
        if (landed.contains(m_rows.at(row).stillPath)) {
            const QModelIndex changed = index(row, 0);
            emit dataChanged(changed, changed, {ArtworkStampRole});
        }
    }
}

QHash<int, QByteArray> ShowEpisodeModel::roleNames() const
{
    return {
        {SeasonRole, "season"},
        {EpisodeRole, "episode"},
        {TitleRole, "title"},
        {OverviewRole, "overview"},
        {AirDateRole, "airDate"},
        {StillPathRole, "stillPath"},
        {RuntimeMinutesRole, "runtimeMinutes"},
        {HandleRole, "handle"},
        {FileNameRole, "fileName"},
        {HasFileRole, "hasFile"},
        {MissingRole, "missing"},
        {WatchedRole, "watched"},
        {PositionSecondsRole, "positionSeconds"},
        {DurationSecondsRole, "durationSeconds"},
        {ProgressRole, "progress"},
        {ResumeSecondsRole, "resumeSeconds"},
        {ArtworkStampRole, "artworkStamp"}
    };
}

qint64 ShowEpisodeModel::mediaId() const
{
    return m_mediaId;
}

void ShowEpisodeModel::setMediaId(qint64 mediaId)
{
    if (m_mediaId == mediaId) {
        return;
    }

    m_mediaId = mediaId;
    emit mediaIdChanged();

    if (m_season != 0) {
        m_season = 0;
        emit seasonChanged();
    }

    reload();
}

int ShowEpisodeModel::season() const
{
    return m_season;
}

void ShowEpisodeModel::setSeason(int season)
{
    if (m_season == season) {
        return;
    }

    m_season = season;
    emit seasonChanged();
    updateSeasonArtwork();
    applySeason();
}

QString ShowEpisodeModel::seasonPosterPath() const
{
    return m_seasonPosterPath;
}

QString ShowEpisodeModel::seasonBackdropPath() const
{
    return m_seasonBackdropPath;
}

void ShowEpisodeModel::updateSeasonArtwork()
{
    const QString poster = m_seasonPosters.value(m_season);

    QString backdrop;
    int firstEpisode = 0;
    for (const EpisodeRecord &episode : std::as_const(m_all)) {
        if (episode.season != m_season || episode.stillPath.isEmpty()) {
            continue;
        }
        if (backdrop.isEmpty() || episode.episode < firstEpisode) {
            backdrop = episode.stillPath;
            firstEpisode = episode.episode;
        }
    }

    if (poster == m_seasonPosterPath && backdrop == m_seasonBackdropPath) {
        return;
    }
    m_seasonPosterPath = poster;
    m_seasonBackdropPath = backdrop;
    emit seasonArtworkChanged();
}

QVariantList ShowEpisodeModel::seasons() const
{
    return m_seasons;
}

QVariantMap ShowEpisodeModel::info() const
{
    return m_info;
}

bool ShowEpisodeModel::hasNextUp() const
{
    return !m_nextUp.isEmpty();
}

QVariantMap ShowEpisodeModel::nextUp() const
{
    return m_nextUp;
}

bool ShowEpisodeModel::seasonWatched() const
{
    return m_seasonWatched;
}

bool ShowEpisodeModel::seasonHasFiles() const
{
    return m_seasonHasFiles;
}

QVariantMap ShowEpisodeModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return QVariantMap();
    }
    return toMap(m_rows.at(row));
}

QStringList ShowEpisodeModel::fileHandles() const
{
    QStringList handles;
    for (const EpisodeRecord &episode : m_all) {
        if (episode.hasFile()) {
            handles.append(episode.fileHandle);
        }
    }
    return handles;
}

QStringList ShowEpisodeModel::fileNames() const
{
    QStringList names;
    for (const EpisodeRecord &episode : m_all) {
        if (episode.hasFile()) {
            names.append(episode.fileName);
        }
    }
    return names;
}

QStringList ShowEpisodeModel::seasonFileHandles() const
{
    QStringList handles;
    for (const EpisodeRecord &episode : m_rows) {
        if (episode.hasFile()) {
            handles.append(episode.fileHandle);
        }
    }
    return handles;
}

int ShowEpisodeModel::episodeCountFor(int season) const
{
    int count = 0;
    for (const EpisodeRecord &episode : m_all) {
        if (episode.season == season) {
            ++count;
        }
    }
    return count;
}

QVariantMap ShowEpisodeModel::toMap(const EpisodeRecord &episode)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("season"), episode.season);
    entry.insert(QStringLiteral("episode"), episode.episode);
    entry.insert(QStringLiteral("title"), episode.title);
    entry.insert(QStringLiteral("overview"), episode.overview);
    entry.insert(QStringLiteral("airDate"), episode.airDate);
    entry.insert(QStringLiteral("stillPath"), episode.stillPath);
    entry.insert(QStringLiteral("runtimeMinutes"), episode.runtimeMinutes);
    entry.insert(QStringLiteral("handle"), episode.fileHandle);
    entry.insert(QStringLiteral("fileName"), episode.fileName);
    entry.insert(QStringLiteral("hasFile"), episode.hasFile());
    entry.insert(QStringLiteral("missing"), episode.missing);
    entry.insert(QStringLiteral("watched"), episode.watched);
    entry.insert(QStringLiteral("positionSeconds"), episode.positionSeconds);
    entry.insert(QStringLiteral("durationSeconds"), episode.durationSeconds);
    entry.insert(QStringLiteral("progress"), progressOf(episode));
    entry.insert(QStringLiteral("resumeSeconds"), resumeOf(episode));
    return entry;
}

void ShowEpisodeModel::reload()
{
    QElapsedTimer timer;
    timer.start();

    MediaRecord media;
    QList<EpisodeRecord> episodes;
    if (m_mediaId > 0) {
        media = m_media.mediaById(m_mediaId);
        if (media.isValid()) {
            episodes = m_media.episodesFor(m_mediaId);
        }
    }

    m_all = episodes;
    m_fileIds.clear();

    QList<int> numbers;
    int onDisk = 0;
    for (const EpisodeRecord &episode : std::as_const(m_all)) {
        if (episode.hasFile()) {
            m_fileIds.insert(episode.fileId);
            ++onDisk;
        }
        if (!numbers.contains(episode.season)) {
            numbers.append(episode.season);
        }
    }
    std::sort(numbers.begin(), numbers.end());
    if (numbers.size() > 1 && numbers.first() == 0) {
        numbers.append(numbers.takeFirst());
    }

    QVariantList seasons;
    for (const int number : std::as_const(numbers)) {
        seasons.append(number);
    }
    if (seasons != m_seasons) {
        m_seasons = seasons;
        emit seasonsChanged();
    }

    QVariantMap info;
    if (media.isValid()) {
        info.insert(QStringLiteral("mediaId"), media.id);
        info.insert(QStringLiteral("tmdbId"), media.tmdbId);
        info.insert(QStringLiteral("title"), media.title);
        info.insert(QStringLiteral("year"), media.year);
        info.insert(QStringLiteral("overview"), media.overview);
        info.insert(QStringLiteral("rating"), media.rating);
        info.insert(QStringLiteral("genres"), media.genres);
        info.insert(QStringLiteral("certification"), media.certification);
        info.insert(QStringLiteral("posterPath"), media.posterPath);
        info.insert(QStringLiteral("backdropPath"), media.backdropPath);
        info.insert(QStringLiteral("seasons"), m_seasons);
        info.insert(QStringLiteral("episodeCount"), int(m_all.size()));
        info.insert(QStringLiteral("filesOnDisk"), onDisk);

        const QList<CreditRecord> credits = m_media.creditsFor(media.id);
        QVariantList cast;
        QStringList creators;
        for (const CreditRecord &credit : credits) {
            if (credit.kind == CreditRecord::Cast) {
                QVariantMap person;
                person.insert(QStringLiteral("name"), credit.name);
                person.insert(QStringLiteral("role"), credit.role);
                person.insert(QStringLiteral("profilePath"), credit.profilePath);
                cast.append(person);
                continue;
            }
            if (credit.kind == CreditRecord::Creator
                && !creators.contains(credit.name)) {
                creators.append(credit.name);
            }
        }
        info.insert(QStringLiteral("cast"), cast);
        info.insert(QStringLiteral("creators"),
                    creators.join(QStringLiteral(", ")));
        info.insert(QStringLiteral("suggested"), m_media.hasSuggestions(media.id));
    }
    if (info != m_info) {
        m_info = info;
        emit infoChanged();
    }

    if (!numbers.isEmpty() && !numbers.contains(m_season)) {
        m_season = numbers.first();
        emit seasonChanged();
    }

    m_seasonPosters = media.isValid() ? m_media.seasonPosters(m_mediaId)
                                      : QHash<int, QString>();
    updateSeasonArtwork();

    const EpisodeRecord *chosen = nullptr;
    for (const EpisodeRecord &episode : std::as_const(m_all)) {
        if (!episode.hasFile() || episode.missing || episode.watched
            || resumeOf(episode) <= 0) {
            continue;
        }
        if (!chosen || episode.lastPlayed > chosen->lastPlayed) {
            chosen = &episode;
        }
    }
    if (!chosen) {
        for (const EpisodeRecord &episode : std::as_const(m_all)) {
            if (episode.hasFile() && !episode.watched) {
                chosen = &episode;
                break;
            }
        }
    }

    const QVariantMap next = chosen ? toMap(*chosen) : QVariantMap();
    if (next != m_nextUp) {
        m_nextUp = next;
        emit nextUpChanged();
    }

    applySeason();

    MM_LOG_D() << "show" << m_mediaId << "read back with" << m_all.size()
               << "episodes in" << timer.elapsed() << "ms";
}

bool ShowEpisodeModel::concerns(const QList<qint64> &mediaIds,
                                const QList<qint64> &fileIds) const
{
    if (m_mediaId <= 0) {
        return false;
    }
    if (mediaIds.contains(m_mediaId)) {
        return true;
    }
    for (const qint64 fileId : fileIds) {
        if (m_fileIds.contains(fileId)) {
            return true;
        }
    }
    return false;
}

void ShowEpisodeModel::applySeason()
{
    QList<EpisodeRecord> next;
    for (const EpisodeRecord &episode : std::as_const(m_all)) {
        if (episode.season == m_season) {
            next.append(episode);
        }
    }

    const bool sameList = m_rowsMediaId == m_mediaId && m_rowsSeason == m_season;
    m_rowsMediaId = m_mediaId;
    m_rowsSeason = m_season;

    if (m_rows == next) {
        updateSeasonState();
        return;
    }

    if (!sameList) {
        beginResetModel();
        m_rows = next;
        endResetModel();
        emit countChanged();
        updateSeasonState();
        return;
    }

    QList<qint64> before;
    before.reserve(m_rows.size());
    for (const EpisodeRecord &episode : std::as_const(m_rows)) {
        before.append(episode.episode);
    }

    QList<qint64> after;
    after.reserve(next.size());
    for (const EpisodeRecord &episode : std::as_const(next)) {
        after.append(episode.episode);
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    if (!ListPatch::worthPatching(steps, int(m_rows.size()), int(next.size()))) {
        beginResetModel();
        m_rows = next;
        endResetModel();
        emit countChanged();
        updateSeasonState();
        return;
    }

    const qsizetype had = m_rows.size();
    int firstChanged = -1;
    int lastChanged = -1;

    const auto flushChanged = [this, &firstChanged, &lastChanged]() {
        if (firstChanged < 0) {
            return;
        }
        emit dataChanged(index(firstChanged, 0), index(lastChanged, 0));
        firstChanged = -1;
        lastChanged = -1;
    };

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Update:
            if (m_rows.at(step.to) != next.at(step.to)) {
                m_rows[step.to] = next.at(step.to);
                if (firstChanged < 0) {
                    firstChanged = step.to;
                }
                lastChanged = step.to;
            } else {
                flushChanged();
            }
            break;

        case ListPatch::Kind::Remove:
            flushChanged();
            beginRemoveRows(QModelIndex(), step.from, step.from + step.count - 1);
            m_rows.remove(step.from, step.count);
            endRemoveRows();
            break;

        case ListPatch::Kind::Insert:
            flushChanged();
            beginInsertRows(QModelIndex(), step.to, step.to + step.count - 1);
            for (int i = 0; i < step.count; ++i) {
                m_rows.insert(step.to + i, next.at(step.to + i));
            }
            endInsertRows();
            break;

        case ListPatch::Kind::Move:
            flushChanged();
            beginMoveRows(QModelIndex(), step.from, step.from, QModelIndex(), step.to);
            m_rows.move(step.from, step.to);
            endMoveRows();
            if (m_rows.at(step.to) != next.at(step.to)) {
                m_rows[step.to] = next.at(step.to);
                const QModelIndex moved = index(step.to, 0);
                emit dataChanged(moved, moved);
            }
            break;
        }
    }

    flushChanged();

    if (m_rows.size() != had) {
        emit countChanged();
    }

    updateSeasonState();
}

void ShowEpisodeModel::updateSeasonState()
{
    int present = 0;
    bool allWatched = true;
    for (const EpisodeRecord &episode : std::as_const(m_rows)) {
        if (!episode.hasFile()) {
            continue;
        }
        ++present;
        if (!episode.watched) {
            allWatched = false;
        }
    }

    const bool watched = present > 0 && allWatched;
    const bool hasFiles = present > 0;
    if (watched == m_seasonWatched && hasFiles == m_seasonHasFiles) {
        return;
    }

    m_seasonWatched = watched;
    m_seasonHasFiles = hasFiles;
    emit seasonStateChanged();
}
