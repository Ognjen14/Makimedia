#include "Library/LibrarySetup.h"

#include "Library/LibraryController.h"
#include "MmLog.h"
#include "Metadata/MetadataService.h"
#include "SystemBridge.h"

#include <QVariantMap>

namespace {
#ifdef Q_OS_ANDROID
constexpr int kSettleMs = 10000;
#else
constexpr int kSettleMs = 500;
#endif
constexpr int kResumeMs = 8000;

const char *stateName(LibrarySetup::State state)
{
    switch (state) {
    case LibrarySetup::State::Running:
        return "running";
    case LibrarySetup::State::Done:
        return "done";
    case LibrarySetup::State::Skipped:
        return "skipped";
    case LibrarySetup::State::Failed:
        return "failed";
    case LibrarySetup::State::Waiting:
    default:
        return "waiting";
    }
}

const char *stepName(LibrarySetup::Step step)
{
    switch (step) {
    case LibrarySetup::Step::Scanning:
        return "scanning folders";
    case LibrarySetup::Step::Reading:
        return "reading filenames";
    case LibrarySetup::Step::Matching:
        return "matching with TMDB";
    case LibrarySetup::Step::Collections:
        return "building collections";
    case LibrarySetup::Step::Artwork:
        return "downloading artwork";
    case LibrarySetup::Step::Building:
    default:
        return "building library";
    }
}
}

LibrarySetup::LibrarySetup(LibraryController &library,
                           MetadataService &metadata,
                           SystemBridge &system,
                           QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_metadata(metadata)
    , m_system(system)
{
    m_settle.setSingleShot(true);
    connect(&m_settle, &QTimer::timeout, this, &LibrarySetup::onSettled);

    m_publish.setSingleShot(true);
    m_publish.setInterval(200);
    connect(&m_publish, &QTimer::timeout, this, &LibrarySetup::progressChanged);

    connect(&m_simulationTimer, &QTimer::timeout, this, &LibrarySetup::simulationTick);

    if (m_store.value(QStringLiteral("librarySetupUnfinished"), false).toBool()) {
        m_resumePending = true;
        MM_LOG_I() << "the last library setup was interrupted, it reopens after the"
                   << "launch scan or in" << kResumeMs << "ms";
        QTimer::singleShot(kResumeMs, this, &LibrarySetup::resumeUnfinished);
    }

    connect(&m_library, &LibraryController::newFilesWritten,
            this, &LibrarySetup::onNewFilesWritten);
    connect(&m_library, &LibraryController::remoteChanged, this, [this]() {
        if (!m_library.isRemote() && m_resumePending) {
            QTimer::singleShot(kResumeMs, this, &LibrarySetup::resumeUnfinished);
        }
    });
    connect(&m_library, &LibraryController::scanQueueFinished,
            this, &LibrarySetup::onScanQueueFinished);

    connect(&m_system, &SystemBridge::storageIndexChanged, this, [this]() {
        if (m_active && m_step == Step::Scanning) {
            holdScanningOpen();
        }
    });
    connect(&m_system, &SystemBridge::readingDrivesChanged, this, [this]() {
        if (m_active && m_step == Step::Scanning) {
            holdScanningOpen();
        }
    });

    connect(&m_metadata, &MetadataService::setupReadProgress,
            this, &LibrarySetup::onReadProgress);
    connect(&m_metadata, &MetadataService::setupReadFinished,
            this, &LibrarySetup::onReadFinished);
    connect(&m_metadata, &MetadataService::setupMatchProgress,
            this, &LibrarySetup::onMatchProgress);
    connect(&m_metadata, &MetadataService::setupMatchFinished,
            this, &LibrarySetup::onMatchFinished);
    connect(&m_metadata, &MetadataService::setupCollectionsProgress,
            this, &LibrarySetup::onCollectionsProgress);
    connect(&m_metadata, &MetadataService::setupCollectionsFinished,
            this, &LibrarySetup::onCollectionsFinished);
    connect(&m_metadata, &MetadataService::setupArtworkProgress,
            this, &LibrarySetup::onArtworkProgress);
    connect(&m_metadata, &MetadataService::setupArtworkFinished,
            this, &LibrarySetup::onArtworkFinished);
}

bool LibrarySetup::active() const
{
    return m_active;
}

bool LibrarySetup::shown() const
{
    return m_active || m_awaitingOk;
}

bool LibrarySetup::finished() const
{
    return m_awaitingOk;
}

QString LibrarySetup::problem() const
{
    return m_problem;
}

int LibrarySetup::round() const
{
    return m_round;
}

int LibrarySetup::roundFiles() const
{
    if (m_round <= 1) {
        return 0;
    }
    return qMax(0, m_steps[int(Step::Scanning)].done - m_roundStart);
}

void LibrarySetup::acknowledge()
{
    if (!m_awaitingOk) {
        return;
    }

    MM_LOG_I() << "library setup window closed with OK";
    m_awaitingOk = false;
    m_problem.clear();
    emit activeChanged();

    if (m_moreArrived) {
        m_moreArrived = false;
        start("more files arrived while setup was running");
        holdScanningOpen();
    }
}

QVariantList LibrarySetup::steps() const
{
    static const char *const titles[] = {
        QT_TR_NOOP("Scanning folders"),
        QT_TR_NOOP("Reading filenames"),
        QT_TR_NOOP("Matching with TMDB"),
        QT_TR_NOOP("Building collections"),
        QT_TR_NOOP("Downloading artwork"),
        QT_TR_NOOP("Building library"),
    };

    QVariantList list;
    for (int i = 0; i < int(Step::Count); ++i) {
        const StepInfo &step = m_steps[i];
        QVariantMap entry;
        entry.insert(QStringLiteral("title"), tr(titles[i]));
        entry.insert(QStringLiteral("state"), QString::fromLatin1(stateName(step.state)));
        entry.insert(QStringLiteral("done"), step.done);
        entry.insert(QStringLiteral("total"), step.total);
        list.append(entry);
    }
    return list;
}

int LibrarySetup::secondsLeft() const
{
    if (!m_active) {
        return -1;
    }

    const StepInfo &step = info(m_step);
    if (step.state != State::Running || step.total <= 0 || step.done < 3
        || !step.clock.isValid() || step.clock.elapsed() < 1500) {
        return -1;
    }

    const double perItemMs = double(step.clock.elapsed()) / step.done;
    return int(perItemMs * (step.total - step.done) / 1000.0 + 0.5);
}

void LibrarySetup::start(const char *why)
{
    stopSimulation();

    if (m_active) {
        return;
    }

    for (StepInfo &step : m_steps) {
        step = StepInfo();
    }

    m_active = true;
    m_awaitingOk = false;
    m_moreArrived = false;
    m_round = 1;
    m_roundStart = 0;
    m_problem.clear();
    m_clock.start();
    MM_LOG_I() << "library setup started:" << why;

    m_resumePending = false;
    m_store.setValue(QStringLiteral("librarySetupUnfinished"), true);

    m_library.setScanRowsHeld(true);
    m_metadata.setHeldForSetup(true);
    run(Step::Scanning);
    info(Step::Scanning).done = 0;

    emit activeChanged();
    emit progressChanged();
}

void LibrarySetup::onNewFilesWritten(int count)
{
    stopSimulation();

    if (!m_active) {
        start("a scan wrote new files");
    }

    info(Step::Scanning).done += count;

    if (m_step != Step::Scanning) {
        if (!m_moreArrived) {
            MM_LOG_I() << "library setup: new files arrived during"
                       << stepName(m_step) << "- they are taken in before building";
        }
        m_moreArrived = true;
        return;
    }

    publishSoon();
    holdScanningOpen();
}

void LibrarySetup::onScanQueueFinished(const QStringList &handles)
{
    if (m_resumePending && !m_active) {
        resumeUnfinished();
    }

    if (!m_active) {
        if (!handles.isEmpty()) {
            m_metadata.matchScanned(handles);
        }
        return;
    }

    if (m_step == Step::Scanning) {
        holdScanningOpen();
    }
}

void LibrarySetup::holdScanningOpen()
{
    if (m_simulating) {
        return;
    }
    m_settle.start(kSettleMs);
}

void LibrarySetup::onSettled()
{
    if (!m_active || m_step != Step::Scanning) {
        return;
    }

    if (m_library.scanning() || !m_system.readingDrives().isEmpty()) {
        MM_LOG_D() << "setup keeps scanning open:"
                   << (m_library.scanning() ? "a scan is running" : "Android is reading a drive");
        holdScanningOpen();
        return;
    }

    StepInfo &scanning = info(Step::Scanning);
    scanning.total = scanning.done;
    complete(Step::Scanning);

    run(Step::Reading);
    m_metadata.readForSetup();
}

void LibrarySetup::onReadProgress(int done, int total)
{
    if (!m_active || m_step != Step::Reading) {
        return;
    }
    StepInfo &reading = info(Step::Reading);
    reading.done = done;
    reading.total = total;
    publishSoon();
}

void LibrarySetup::onReadFinished(int files, int titles)
{
    if (!m_active || m_step != Step::Reading) {
        return;
    }
    Q_UNUSED(titles)

    complete(Step::Reading);

    if (!m_metadata.available()) {
        MM_LOG_I() << "setup skips matching and artwork: no TMDB key";
        complete(Step::Matching, State::Skipped);
        complete(Step::Collections, State::Skipped);
        complete(Step::Artwork, State::Skipped);
        build();
        return;
    }

    if (files == 0) {
        MM_LOG_I() << "setup skips matching, nothing to ask TMDB about;"
                   << "collections and artwork still check for anything missing";
        complete(Step::Matching, State::Skipped);
        runCollections();
        return;
    }

    run(Step::Matching);
    m_metadata.matchForSetup(m_system.isTelevision());
}

void LibrarySetup::onMatchProgress(int done, int total)
{
    if (!m_active || m_step != Step::Matching) {
        return;
    }
    StepInfo &matching = info(Step::Matching);
    matching.done = done;
    matching.total = total;
    publishSoon();
}

void LibrarySetup::onMatchFinished(int unreachable)
{
    if (!m_active || m_step != Step::Matching) {
        return;
    }

    const int total = info(Step::Matching).total;
    if (unreachable > 0 && unreachable >= total) {
        addProblem(unreachableProblem(unreachable, true));
        complete(Step::Matching, State::Failed);
        complete(Step::Collections, State::Skipped);
        complete(Step::Artwork, State::Skipped);
        build();
        return;
    }

    if (unreachable > 0) {
        addProblem(unreachableProblem(unreachable, false));
    }

    complete(Step::Matching);
    runCollections();
}

void LibrarySetup::runCollections()
{
    run(Step::Collections);
    m_metadata.collectionsForSetup(m_system.isTelevision());
}

void LibrarySetup::onCollectionsProgress(int done, int total)
{
    if (!m_active || m_step != Step::Collections) {
        return;
    }
    StepInfo &collections = info(Step::Collections);
    collections.done = done;
    collections.total = total;
    publishSoon();
}

void LibrarySetup::onCollectionsFinished(int failed)
{
    if (!m_active || m_step != Step::Collections) {
        return;
    }

    const int total = info(Step::Collections).total;
    if (failed > 0) {
        addProblem(collectionsProblem(failed));
    }
    complete(Step::Collections, failed > 0 && failed >= total ? State::Failed : State::Done);

    run(Step::Artwork);
    m_metadata.downloadArtworkForSetup(m_system.isTelevision());
}

QString LibrarySetup::collectionsProblem(int failed)
{
    return tr("%n collection detail(s) could not be fetched. They are asked for "
              "again later.", "", failed);
}

void LibrarySetup::onArtworkProgress(int done, int total)
{
    if (!m_active || m_step != Step::Artwork) {
        return;
    }
    StepInfo &artwork = info(Step::Artwork);
    artwork.done = done;
    artwork.total = total;
    publishSoon();
}

void LibrarySetup::onArtworkFinished(int failed)
{
    if (!m_active || m_step != Step::Artwork) {
        return;
    }

    const int total = info(Step::Artwork).total;
    if (failed > 0 && failed >= total) {
        addProblem(artworkProblem(failed, true));
        complete(Step::Artwork, State::Failed);
    } else {
        if (failed > 0) {
            addProblem(artworkProblem(failed, false));
        }
        complete(Step::Artwork);
    }

    build();
}

QString LibrarySetup::unreachableProblem(int unreachable, bool all)
{
    if (all) {
        return tr("TMDB could not be reached, so nothing was matched. "
                  "Your videos are matched when the connection is back.");
    }
    return tr("TMDB could not be reached for %n video(s). "
              "They are matched when the connection is back.", "", unreachable);
}

QString LibrarySetup::artworkProblem(int failed, bool all)
{
    if (all) {
        return tr("No artwork could be downloaded. It is fetched when the pages "
                  "are opened.");
    }
    return tr("%n image(s) could not be downloaded. They are fetched when "
              "the pages are opened.", "", failed);
}

void LibrarySetup::simulate(const QString &scenario)
{
    if (m_active && !m_simulating) {
        MM_LOG_W() << "a real library setup is running, not simulating" << scenario;
        return;
    }

    stopSimulation();

    for (StepInfo &step : m_steps) {
        step = StepInfo();
    }

    m_simulating = true;
    m_simulation = scenario;
    m_simulationLooped = false;
    m_simulationTarget = 1200;
    m_simulationBatch = 1200;
    m_simulationBuildTicks = 0;

    m_active = true;
    m_awaitingOk = false;
    m_moreArrived = false;
    m_round = 1;
    m_roundStart = 0;
    m_problem.clear();
    m_clock.start();
    MM_LOG_I() << "simulated library setup started:" << scenario
               << "- nothing is scanned, matched or downloaded";

    run(Step::Scanning);
    info(Step::Scanning).done = 0;
    emit activeChanged();
    emit progressChanged();

    m_simulationTimer.start(100);
}

void LibrarySetup::stopSimulation()
{
    if (!m_simulating) {
        return;
    }

    m_simulationTimer.stop();
    m_simulating = false;
    m_active = false;
    m_awaitingOk = false;
    MM_LOG_I() << "simulated library setup stopped";
    emit activeChanged();
}

void LibrarySetup::simulationTick()
{
    if (!m_simulating) {
        m_simulationTimer.stop();
        return;
    }

    const auto advance = [this](Step step, int by, int total) {
        StepInfo &current = info(step);
        current.total = total;
        current.done = qMin(total, current.done + qMax(1, by));
        publishSoon();
        return current.done >= total;
    };

    switch (m_step) {
    case Step::Scanning: {
        StepInfo &scanning = info(Step::Scanning);
        scanning.done = qMin(m_simulationTarget, scanning.done + qMax(1, m_simulationBatch / 32));
        publishSoon();
        if (scanning.done < m_simulationTarget) {
            return;
        }
        scanning.total = scanning.done;
        complete(Step::Scanning);
        run(Step::Reading);
        return;
    }

    case Step::Reading:
        if (advance(Step::Reading, m_simulationBatch / 8, m_simulationBatch)) {
            complete(Step::Reading);
            run(Step::Matching);
        }
        return;

    case Step::Matching:
        if (!advance(Step::Matching, m_simulationBatch / 60, m_simulationBatch)) {
            return;
        }
        if (m_simulation == QLatin1String("unreachable")) {
            addProblem(unreachableProblem(m_simulationBatch, true));
            complete(Step::Matching, State::Failed);
            complete(Step::Collections, State::Skipped);
            complete(Step::Artwork, State::Skipped);
            m_simulationBuildTicks = 6;
            run(Step::Building);
            return;
        }
        complete(Step::Matching);
        run(Step::Collections);
        return;

    case Step::Collections:
        if (advance(Step::Collections, m_simulationBatch / 100, m_simulationBatch / 5)) {
            complete(Step::Collections);
            run(Step::Artwork);
        }
        return;

    case Step::Artwork:
        if (!advance(Step::Artwork, m_simulationBatch / 20, m_simulationBatch * 2)) {
            return;
        }
        if (m_simulation == QLatin1String("artwork") && !m_simulationLooped) {
            addProblem(artworkProblem(37, false));
        }
        complete(Step::Artwork);

        if (m_simulation == QLatin1String("more") && !m_simulationLooped) {
            MM_LOG_I() << "simulated library setup: more files arrived, back to scanning";
            m_simulationLooped = true;
            m_simulationBatch = 120;
            ++m_round;
            m_roundStart = info(Step::Scanning).done;
            m_simulationTarget = m_roundStart + 120;
            info(Step::Reading) = StepInfo();
            info(Step::Matching) = StepInfo();
            info(Step::Collections) = StepInfo();
            info(Step::Artwork) = StepInfo();
            info(Step::Scanning).total = -1;
            run(Step::Scanning);
            return;
        }

        m_simulationBuildTicks = 6;
        run(Step::Building);
        return;

    case Step::Building:
        if (--m_simulationBuildTicks > 0) {
            return;
        }
        m_simulationTimer.stop();
        m_simulating = false;
        finish();
        return;

    case Step::Count:
    default:
        return;
    }
}

void LibrarySetup::addProblem(const QString &text)
{
    if (!m_problem.isEmpty()) {
        m_problem += QLatin1Char(' ');
    }
    m_problem += text;
}

void LibrarySetup::build()
{
    if (m_moreArrived) {
        m_moreArrived = false;
        ++m_round;
        m_roundStart = qMax(0, info(Step::Scanning).total);
        MM_LOG_I() << "library setup: going back to scanning for the files that"
                   << "arrived, the window stays open, round" << m_round;
        info(Step::Reading) = StepInfo();
        info(Step::Matching) = StepInfo();
        info(Step::Collections) = StepInfo();
        info(Step::Artwork) = StepInfo();
        info(Step::Scanning).total = -1;
        run(Step::Scanning);
        holdScanningOpen();
        return;
    }

    run(Step::Building);
    m_library.setScanRowsHeld(false);
    m_metadata.setHeldForSetup(false);
    m_store.remove(QStringLiteral("librarySetupUnfinished"));
    QTimer::singleShot(0, this, &LibrarySetup::finish);
}

void LibrarySetup::resumeUnfinished()
{
    if (!m_resumePending) {
        return;
    }
    if (m_library.isRemote() || m_library.heldStill()) {
        MM_LOG_I() << "the interrupted setup belongs to this device's own library,"
                   << "so it waits until that library is back";
        return;
    }
    m_resumePending = false;

    if (m_active && !m_simulating) {
        return;
    }

    MM_LOG_I() << "the last library setup did not finish, opening it again";
    start("the last setup was interrupted");
    holdScanningOpen();
}

void LibrarySetup::finish()
{
    complete(Step::Building);

    MM_LOG_I() << "library setup finished in" << m_clock.elapsed() << "ms"
               << (m_problem.isEmpty() ? "" : "with a problem:") << m_problem;
    m_active = false;
    m_awaitingOk = true;
    m_settle.stop();
    m_publish.stop();
    emit progressChanged();
    emit activeChanged();
}

void LibrarySetup::run(Step step)
{
    m_step = step;
    StepInfo &current = info(step);
    current.state = State::Running;
    current.clock.start();
    MM_LOG_I() << "library setup:" << stepName(step) << "started";
    publishSoon();
}

void LibrarySetup::complete(Step step, State state)
{
    StepInfo &current = info(step);
    current.state = state;
    if (current.total < 0) {
        current.total = current.done;
    }
    if (state == State::Done) {
        current.done = current.total;
    }
    MM_LOG_I() << "library setup:" << stepName(step) << stateName(state)
               << "with" << current.total << "in"
               << (current.clock.isValid() ? current.clock.elapsed() : 0) << "ms";
    publishSoon();
}

LibrarySetup::StepInfo &LibrarySetup::info(Step step)
{
    return m_steps[int(step)];
}

const LibrarySetup::StepInfo &LibrarySetup::info(Step step) const
{
    return m_steps[int(step)];
}

void LibrarySetup::publishSoon()
{
    if (!m_publish.isActive()) {
        m_publish.start();
    }
}
