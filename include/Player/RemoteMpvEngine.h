#pragma once

#include "Player/MpvEngine.h"
#include "Player/MpvWire.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

class RemoteMpvEngine final : public MpvEngine
{
public:
    enum Kind { Playback = 0, Preview = 1 };

    explicit RemoteMpvEngine(Kind kind);
    ~RemoteMpvEngine() override;

    static bool unavailable();

    bool remote() const override { return true; }
    void abandon() override;
    bool alive() const override;

    int setOptionString(const char *name, const char *value) override;
    int initialize() override;
    int requestLogMessages(const char *level) override;
    int observeProperty(uint64_t userdata, const char *name, mpv_format format) override;
    void setWakeupCallback(void (*callback)(void *), void *context) override;
    int hookAdd(uint64_t userdata, const char *name, int priority) override;
    int hookContinue(uint64_t id) override;
    int command(const char **args) override;
    int setPropertyString(const char *name, const char *value) override;
    int getDouble(const char *name, double *out) override;
    int getInt64(const char *name, int64_t *out) override;
    bool getString(const char *name, QByteArray *out) override;
    bool getVariant(const char *name, QVariant *out) override;
    int setWindow(qint64 windowHandle) override;
    mpv_event *waitEvent(double timeout) override;

private:
    struct Answer
    {
        bool done = false;
        int rc = MPV_ERROR_GENERIC;
        mpvwire::Arena arena;
        mpvwire::Value value;
    };

    const char *name() const;
    mpvwire::Writer request(mpvwire::Op op, uint32_t *id);
    int send(uint32_t id, mpvwire::Writer message, const std::string &what);
    int ask(uint32_t id, mpvwire::Writer message, const std::string &what,
            std::shared_ptr<Answer> *answer = nullptr, int timeoutMs = 5000);
    bool waitConnected(int timeoutMs);
    void connectAndWrite();
    void readAnswers();
    void lose(const char *why);
    void wake();
    void letGoOfProcess();

    const Kind m_kind;
    int m_fd = -1;
    int m_peerFd = -1;
    std::atomic<int> m_pid{-1};
    std::atomic<uint32_t> m_nextId{1};
    std::atomic<bool> m_abandoned{false};

    mutable std::mutex m_lock;
    std::condition_variable m_changed;
    bool m_connected = false;
    bool m_lost = false;
    bool m_stopping = false;
    std::deque<std::string> m_outgoing;
    std::unordered_map<uint32_t, std::shared_ptr<Answer>> m_waiting;
    std::unordered_map<uint32_t, std::string> m_unwatched;
    std::deque<std::unique_ptr<mpvwire::DecodedEvent>> m_events;

    std::unique_ptr<mpvwire::DecodedEvent> m_current;
    mpv_event m_none{};

    std::mutex m_wakeLock;
    void (*m_wakeup)(void *) = nullptr;
    void *m_wakeContext = nullptr;

    std::thread m_writer;
    std::thread m_reader;
};
