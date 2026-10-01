#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariantList>

class LibraryController;
class MetadataService;
class SystemBridge;

class LibrarySetup : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool shown READ shown NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool finished READ finished NOTIFY activeChanged FINAL)
    Q_PROPERTY(QString problem READ problem NOTIFY activeChanged FINAL)
    Q_PROPERTY(QVariantList steps READ steps NOTIFY progressChanged FINAL)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY progressChanged FINAL)
    Q_PROPERTY(int round READ round NOTIFY progressChanged FINAL)
    Q_PROPERTY(int roundFiles READ roundFiles NOTIFY progressChanged FINAL)

public:
    enum class Step { Scanning, Reading, Matching, Collections, Artwork, Building, Count };
    enum class State { Waiting, Running, Done, Skipped, Failed };

    LibrarySetup(LibraryController &library,
                 MetadataService &metadata,
                 SystemBridge &system,
                 QObject *parent = nullptr);

    bool active() const;
    bool shown() const;
    bool finished() const;
    QString problem() const;
    QVariantList steps() const;
    int secondsLeft() const;
    int round() const;
    int roundFiles() const;

    Q_INVOKABLE void acknowledge();
    Q_INVOKABLE void simulate(const QString &scenario);

signals:
    void activeChanged();
    void progressChanged();

private:
    struct StepInfo
    {
        State state = State::Waiting;
        int done = 0;
        int total = -1;
        QElapsedTimer clock;
    };

    void start(const char *why);
    void onNewFilesWritten(int count);
    void onScanQueueFinished(const QStringList &handles);
    void holdScanningOpen();
    void onSettled();
    void onReadProgress(int done, int total);
    void onReadFinished(int files, int titles);
    void onMatchProgress(int done, int total);
    void onMatchFinished(int unreachable);
    void runCollections();
    void onCollectionsProgress(int done, int total);
    void onCollectionsFinished(int failed);
    void onArtworkProgress(int done, int total);
    void onArtworkFinished(int failed);
    void addProblem(const QString &text);
    static QString unreachableProblem(int unreachable, bool all);
    static QString artworkProblem(int failed, bool all);
    static QString collectionsProblem(int failed);

    void simulationTick();
    void stopSimulation();
    void resumeUnfinished();
    void build();
    void finish();

    void run(Step step);
    void complete(Step step, State state = State::Done);
    StepInfo &info(Step step);
    const StepInfo &info(Step step) const;
    void publishSoon();

    LibraryController &m_library;
    MetadataService &m_metadata;
    SystemBridge &m_system;

    bool m_active = false;
    bool m_awaitingOk = false;
    bool m_moreArrived = false;
    int m_round = 1;
    int m_roundStart = 0;
    bool m_resumePending = false;
    QSettings m_store;
    QString m_problem;
    Step m_step = Step::Scanning;
    StepInfo m_steps[int(Step::Count)];
    QElapsedTimer m_clock;
    QTimer m_settle;
    QTimer m_publish;

    bool m_simulating = false;
    QString m_simulation;
    bool m_simulationLooped = false;
    int m_simulationTarget = 0;
    int m_simulationBatch = 0;
    int m_simulationBuildTicks = 0;
    QTimer m_simulationTimer;
};
