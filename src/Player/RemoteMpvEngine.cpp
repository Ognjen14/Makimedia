#include "Player/RemoteMpvEngine.h"

#include "MmLog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJniEnvironment>
#include <QJniObject>

#include <chrono>
#include <cerrno>
#include <vector>

#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

const char *kClientClass = "com/topicdev/makimedia/org/MakimediaPlayerClient";

std::atomic<bool> s_unavailable{false};

constexpr int kConnectWaitMs = 35000;

QJniObject appContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

}

bool RemoteMpvEngine::unavailable()
{
    return s_unavailable.load();
}

RemoteMpvEngine::RemoteMpvEngine(Kind kind)
    : m_kind(kind)
{
    int fds[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds) != 0) {
        MM_LOG_E() << "no socket pair for the" << name() << "player process:" << errno;
        m_lost = true;
        return;
    }
    const int size = mpvwire::kSocketBuffer;
    for (int fd : fds) {
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
    }
    m_fd = fds[0];
    m_peerFd = fds[1];

    m_writer = std::thread([this]() { connectAndWrite(); });
}

RemoteMpvEngine::~RemoteMpvEngine()
{
    setWakeupCallback(nullptr, nullptr);
    m_abandoned = true;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stopping = true;
    }
    m_changed.notify_all();

    letGoOfProcess();
    if (m_fd >= 0) {
        shutdown(m_fd, SHUT_RDWR);
    }
    if (m_writer.joinable()) {
        m_writer.join();
    }
    if (m_reader.joinable()) {
        m_reader.join();
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
    if (m_peerFd >= 0) {
        close(m_peerFd);
    }
}

const char *RemoteMpvEngine::name() const
{
    return m_kind == Preview ? "preview" : "playback";
}

void RemoteMpvEngine::letGoOfProcess()
{
    const int pid = m_pid.exchange(-1);
    if (pid <= 0) {
        return;
    }
    kill(pid, SIGKILL);
    QJniObject context = appContext();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>(kClientClass, "disconnect",
                                           "(Landroid/content/Context;II)V",
                                           context.object(), jint(m_kind), jint(pid));
    }
    MM_LOG_I() << "ended the" << name() << "player process" << pid;
}

void RemoteMpvEngine::abandon()
{
    m_abandoned = true;
    letGoOfProcess();
    if (m_fd >= 0) {
        shutdown(m_fd, SHUT_RDWR);
    }
    lose("it was abandoned");
}

bool RemoteMpvEngine::alive() const
{
    std::lock_guard<std::mutex> lock(m_lock);
    return !m_lost;
}

void RemoteMpvEngine::connectAndWrite()
{
    QElapsedTimer took;
    took.start();

    QJniObject context = appContext();
    jint pid = -1;
    if (context.isValid()) {
        pid = QJniObject::callStaticMethod<jint>(kClientClass, "connect",
                                                 "(Landroid/content/Context;II)I",
                                                 context.object(), jint(m_kind),
                                                 jint(m_peerFd));
    }
    close(m_peerFd);
    m_peerFd = -1;

    if (pid <= 0) {
        MM_LOG_E() << "the" << name() << "player process could not be started -"
                   << "playing in the app's own process from now on";
        s_unavailable = true;
        lose("it never started");
        return;
    }

    m_pid = pid;
    MM_LOG_I() << "the" << name() << "player process" << pid << "is up after"
               << took.elapsed() << "ms";

    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_connected = true;
    }
    m_changed.notify_all();

    if (m_abandoned) {
        letGoOfProcess();
    }

    m_reader = std::thread([this]() { readAnswers(); });

    while (true) {
        std::string message;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_changed.wait(lock, [this]() {
                return m_stopping || m_lost || !m_outgoing.empty();
            });
            if (m_stopping || m_lost) {
                return;
            }
            message = std::move(m_outgoing.front());
            m_outgoing.pop_front();
        }
        if (::send(m_fd, message.data(), message.size(), MSG_NOSIGNAL) < 0) {
            lose("a message could not be sent");
            return;
        }
    }
}

void RemoteMpvEngine::readAnswers()
{
    std::vector<char> buffer(mpvwire::kMaxMessage);
    while (true) {
        const ssize_t size = recv(m_fd, buffer.data(), buffer.size(), 0);
        if (size <= 0) {
            lose(size == 0 ? "its process ended" : "its socket failed");
            return;
        }

        mpvwire::Reader in(buffer.data(), static_cast<size_t>(size));
        const auto kind = static_cast<mpvwire::Kind>(in.u32());

        if (kind == mpvwire::Kind::Event) {
            auto event = std::make_unique<mpvwire::DecodedEvent>();
            if (!mpvwire::decodeEvent(in, event.get())) {
                MM_LOG_W() << "an unreadable event from the" << name() << "player process";
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(m_lock);
                m_events.push_back(std::move(event));
            }
            wake();
            continue;
        }

        if (kind != mpvwire::Kind::Reply) {
            continue;
        }

        const uint32_t id = in.u32();
        const int rc = in.i32();
        std::string unwatched;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            const auto waiting = m_waiting.find(id);
            if (waiting != m_waiting.end()) {
                const std::shared_ptr<Answer> answer = waiting->second;
                answer->rc = rc;
                mpvwire::decodeValue(in, answer->arena, &answer->value);
                answer->done = true;
                m_waiting.erase(waiting);
            } else {
                const auto sent = m_unwatched.find(id);
                if (sent != m_unwatched.end()) {
                    unwatched = std::move(sent->second);
                    m_unwatched.erase(sent);
                }
            }
        }
        m_changed.notify_all();

        if (rc < 0 && !unwatched.empty()) {
            MM_LOG_W() << "the" << name() << "player process refused"
                       << QString::fromStdString(unwatched) << ":" << mpv_error_string(rc);
        }
    }
}

void RemoteMpvEngine::lose(const char *why)
{
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_lost) {
            return;
        }
        m_lost = true;
        m_waiting.clear();
        m_unwatched.clear();
    }
    m_changed.notify_all();
    if (!m_stopping) {
        MM_LOG_W() << "lost the" << name() << "player process:" << why;
    }
    wake();
}

void RemoteMpvEngine::wake()
{
    std::lock_guard<std::mutex> lock(m_wakeLock);
    if (m_wakeup) {
        m_wakeup(m_wakeContext);
    }
}

void RemoteMpvEngine::setWakeupCallback(void (*callback)(void *), void *context)
{
    {
        std::lock_guard<std::mutex> lock(m_wakeLock);
        m_wakeup = callback;
        m_wakeContext = context;
    }
    if (callback) {
        bool pending = false;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            pending = !m_events.empty();
        }
        if (pending) {
            wake();
        }
    }
}

mpvwire::Writer RemoteMpvEngine::request(mpvwire::Op op, uint32_t *id)
{
    *id = m_nextId.fetch_add(1);
    mpvwire::Writer message;
    message.u32(static_cast<uint32_t>(mpvwire::Kind::Request));
    message.u32(*id);
    message.u32(static_cast<uint32_t>(op));
    return message;
}

int RemoteMpvEngine::send(uint32_t id, mpvwire::Writer message, const std::string &what)
{
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_lost) {
            return MPV_ERROR_UNINITIALIZED;
        }
        m_unwatched.emplace(id, what);
        m_outgoing.push_back(message.bytes());
    }
    m_changed.notify_all();
    return 0;
}

bool RemoteMpvEngine::waitConnected(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_lock);
    m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
        return m_connected || m_lost || m_stopping;
    });
    return m_connected && !m_lost;
}

int RemoteMpvEngine::ask(uint32_t id, mpvwire::Writer message, const std::string &what,
                         std::shared_ptr<Answer> *answer, int timeoutMs)
{
    if (!waitConnected(kConnectWaitMs)) {
        return MPV_ERROR_UNINITIALIZED;
    }

    auto pending = std::make_shared<Answer>();
    std::unique_lock<std::mutex> lock(m_lock);
    if (m_lost) {
        return MPV_ERROR_UNINITIALIZED;
    }
    m_waiting.emplace(id, pending);
    m_outgoing.push_back(message.bytes());
    m_changed.notify_all();

    const bool answered = m_changed.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                             [&]() { return pending->done || m_lost; });
    if (!pending->done) {
        m_waiting.erase(id);
        lock.unlock();
        if (!answered) {
            MM_LOG_W() << "the" << name() << "player process did not answer"
                       << QString::fromStdString(what) << "in" << timeoutMs << "ms";
        }
        return MPV_ERROR_GENERIC;
    }
    if (answer) {
        *answer = pending;
    }
    return pending->rc;
}

int RemoteMpvEngine::setOptionString(const char *name, const char *value)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::SetOptionString, &id);
    message.str(name);
    message.str(value);
    return send(id, std::move(message), std::string("option ") + name);
}

int RemoteMpvEngine::initialize()
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::Initialize, &id);
    return send(id, std::move(message), "initialise");
}

int RemoteMpvEngine::requestLogMessages(const char *level)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::RequestLogMessages, &id);
    message.str(level);
    return send(id, std::move(message), "log messages");
}

int RemoteMpvEngine::observeProperty(uint64_t userdata, const char *name, mpv_format format)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::ObserveProperty, &id);
    message.u64(userdata);
    message.str(name);
    message.i32(format);
    return send(id, std::move(message), std::string("observing ") + name);
}

int RemoteMpvEngine::hookAdd(uint64_t userdata, const char *name, int priority)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::HookAdd, &id);
    message.u64(userdata);
    message.str(name);
    message.i32(priority);
    return send(id, std::move(message), std::string("hook ") + name);
}

int RemoteMpvEngine::hookContinue(uint64_t hookId)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::HookContinue, &id);
    message.u64(hookId);
    return send(id, std::move(message), "hook continue");
}

int RemoteMpvEngine::command(const char **args)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::Command, &id);
    uint32_t count = 0;
    while (args[count]) {
        ++count;
    }
    message.u32(count);
    for (uint32_t i = 0; i < count; ++i) {
        message.str(args[i]);
    }
    return ask(id, std::move(message), std::string("command ") + (count ? args[0] : ""));
}

int RemoteMpvEngine::setPropertyString(const char *name, const char *value)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::SetPropertyString, &id);
    message.str(name);
    message.str(value);
    return ask(id, std::move(message), std::string("setting ") + name);
}

int RemoteMpvEngine::getDouble(const char *name, double *out)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::GetProperty, &id);
    message.str(name);
    message.i32(MPV_FORMAT_DOUBLE);
    std::shared_ptr<Answer> answer;
    const int rc = ask(id, std::move(message), std::string("reading ") + name, &answer);
    if (rc < 0 || !answer || !answer->value.present
        || answer->value.format != MPV_FORMAT_DOUBLE) {
        return rc < 0 ? rc : MPV_ERROR_PROPERTY_UNAVAILABLE;
    }
    *out = answer->value.double_;
    return rc;
}

int RemoteMpvEngine::getInt64(const char *name, int64_t *out)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::GetProperty, &id);
    message.str(name);
    message.i32(MPV_FORMAT_INT64);
    std::shared_ptr<Answer> answer;
    const int rc = ask(id, std::move(message), std::string("reading ") + name, &answer);
    if (rc < 0 || !answer || !answer->value.present
        || answer->value.format != MPV_FORMAT_INT64) {
        return rc < 0 ? rc : MPV_ERROR_PROPERTY_UNAVAILABLE;
    }
    *out = answer->value.int64;
    return rc;
}

bool RemoteMpvEngine::getString(const char *name, QByteArray *out)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::GetPropertyString, &id);
    message.str(name);
    std::shared_ptr<Answer> answer;
    const int rc = ask(id, std::move(message), std::string("reading ") + name, &answer);
    if (rc < 0 || !answer || !answer->value.present || !answer->value.string) {
        return false;
    }
    *out = QByteArray(answer->value.string);
    return true;
}

bool RemoteMpvEngine::getVariant(const char *name, QVariant *out)
{
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::GetProperty, &id);
    message.str(name);
    message.i32(MPV_FORMAT_NODE);
    std::shared_ptr<Answer> answer;
    const int rc = ask(id, std::move(message), std::string("reading ") + name, &answer);
    if (rc < 0 || !answer || !answer->value.present
        || answer->value.format != MPV_FORMAT_NODE) {
        return false;
    }
    *out = mpvNodeToVariant(&answer->value.node);
    return true;
}

int RemoteMpvEngine::setWindow(qint64)
{
    if (!waitConnected(kConnectWaitMs)) {
        return MPV_ERROR_UNINITIALIZED;
    }
    const bool handed = QJniObject::callStaticMethod<jboolean>(kClientClass, "sendSurface",
                                                               "(I)Z", jint(m_kind));
    if (!handed) {
        MM_LOG_E() << "the" << name() << "surface could not be handed to its player process";
        return MPV_ERROR_INVALID_PARAMETER;
    }
    uint32_t id = 0;
    mpvwire::Writer message = request(mpvwire::Op::SetWindow, &id);
    return ask(id, std::move(message), "the video surface");
}

mpv_event *RemoteMpvEngine::waitEvent(double)
{
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_events.empty()) {
        m_current.reset();
        m_none = mpv_event{};
        return &m_none;
    }
    m_current = std::move(m_events.front());
    m_events.pop_front();
    return &m_current->event;
}
