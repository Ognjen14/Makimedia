
#include "Player/MpvWire.h"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

const char *kTag = "MakimediaPlayerHost";

JavaVM *g_vm = nullptr;

std::mutex g_surfaceLock;
jobject g_surface = nullptr;

void handOverJavaVm()
{
    using SetJavaVmFn = int (*)(void *, void *);
    SetJavaVmFn setJavaVm = nullptr;

    void *avcodec = dlopen("libavcodec.so", RTLD_NOW | RTLD_NOLOAD);
    if (avcodec) {
        setJavaVm = reinterpret_cast<SetJavaVmFn>(dlsym(avcodec, "av_jni_set_java_vm"));
    }
    if (!setJavaVm) {
        setJavaVm = reinterpret_cast<SetJavaVmFn>(dlsym(RTLD_DEFAULT, "av_jni_set_java_vm"));
    }
    if (!setJavaVm) {
        __android_log_print(ANDROID_LOG_ERROR, kTag,
                            "av_jni_set_java_vm not found, MediaCodec is out of reach");
        return;
    }
    const int rc = setJavaVm(g_vm, nullptr);
    __android_log_print(rc == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, kTag,
                        "JavaVM handed to FFmpeg, rc %d", rc);
}

class Session
{
public:
    explicit Session(int fd) : m_fd(fd), m_buffer(mpvwire::kMaxMessage) {}

    void start()
    {
        m_mpv = mpv_create();
        if (!m_mpv) {
            __android_log_print(ANDROID_LOG_ERROR, kTag, "mpv_create failed");
        }
        std::thread([this]() { work(); }).detach();
        std::thread([this]() { read(); }).detach();
        note("player process %d ready", static_cast<int>(getpid()));
    }

    void note(const char *format, ...)
    {
        char text[512];
        va_list args;
        va_start(args, format);
        vsnprintf(text, sizeof text, format, args);
        va_end(args);
        __android_log_print(ANDROID_LOG_INFO, kTag, "%s", text);

        mpv_event_log_message message{};
        message.prefix = "host";
        message.level = "info";
        message.text = text;
        message.log_level = MPV_LOG_LEVEL_INFO;
        mpv_event event{};
        event.event_id = MPV_EVENT_LOG_MESSAGE;
        event.data = &message;
        mpvwire::Writer out;
        mpvwire::encodeEvent(out, &event);
        send(out);
    }

private:
    struct Request
    {
        uint32_t id = 0;
        std::string bytes;
    };

    [[noreturn]] void endProcess(const char *why)
    {
        __android_log_print(ANDROID_LOG_INFO, kTag, "ending the player process: %s", why);
        _exit(0);
    }

    void send(const mpvwire::Writer &out)
    {
        std::lock_guard<std::mutex> lock(m_sendLock);
        const std::string &bytes = out.bytes();
        if (::send(m_fd, bytes.data(), bytes.size(), MSG_NOSIGNAL) < 0) {
            endProcess("the app's socket is closed");
        }
    }

    void read()
    {
        while (true) {
            const ssize_t size = recv(m_fd, m_buffer.data(), m_buffer.size(), 0);
            if (size <= 0) {
                endProcess(size == 0 ? "the app closed its end" : "the socket failed");
            }
            mpvwire::Reader in(m_buffer.data(), static_cast<size_t>(size));
            if (static_cast<mpvwire::Kind>(in.u32()) != mpvwire::Kind::Request) {
                continue;
            }
            Request request;
            request.id = in.u32();
            request.bytes.assign(m_buffer.data(), static_cast<size_t>(size));
            if (!in.ok()) {
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(m_queueLock);
                m_queue.push_back(std::move(request));
            }
            m_queueReady.notify_one();
        }
    }

    void work()
    {
        while (true) {
            Request request;
            {
                std::unique_lock<std::mutex> lock(m_queueLock);
                m_queueReady.wait(lock, [this]() { return !m_queue.empty(); });
                request = std::move(m_queue.front());
                m_queue.pop_front();
            }
            run(request);
        }
    }

    void run(const Request &request)
    {
        mpvwire::Reader in(request.bytes.data(), request.bytes.size());
        in.u32();
        in.u32();
        const auto op = static_cast<mpvwire::Op>(in.u32());

        mpvwire::Writer reply;
        reply.u32(static_cast<uint32_t>(mpvwire::Kind::Reply));
        reply.u32(request.id);

        if (op == mpvwire::Op::Quit) {
            endProcess("the app is done with it");
        }

        if (!m_mpv) {
            reply.i32(MPV_ERROR_UNINITIALIZED);
            reply.value(MPV_FORMAT_NONE, nullptr);
            send(reply);
            return;
        }

        std::string a;
        std::string b;
        int rc = MPV_ERROR_INVALID_PARAMETER;

        switch (op) {
        case mpvwire::Op::SetOptionString:
            in.str(&a);
            in.str(&b);
            rc = mpv_set_option_string(m_mpv, a.c_str(), b.c_str());
            break;
        case mpvwire::Op::Initialize:
            rc = mpv_initialize(m_mpv);
            if (rc >= 0 && !m_eventsRunning) {
                m_eventsRunning = true;
                std::thread([this]() { forwardEvents(); }).detach();
            }
            break;
        case mpvwire::Op::RequestLogMessages:
            in.str(&a);
            rc = mpv_request_log_messages(m_mpv, a.c_str());
            break;
        case mpvwire::Op::ObserveProperty: {
            const uint64_t userdata = in.u64();
            in.str(&a);
            const auto format = static_cast<mpv_format>(in.i32());
            rc = mpv_observe_property(m_mpv, userdata, a.c_str(), format);
            break;
        }
        case mpvwire::Op::HookAdd: {
            const uint64_t userdata = in.u64();
            in.str(&a);
            const int priority = in.i32();
            rc = mpv_hook_add(m_mpv, userdata, a.c_str(), priority);
            break;
        }
        case mpvwire::Op::HookContinue:
            rc = mpv_hook_continue(m_mpv, in.u64());
            break;
        case mpvwire::Op::Command: {
            const uint32_t count = in.u32();
            std::vector<std::string> words;
            for (uint32_t i = 0; in.ok() && i < count && i < 64; ++i) {
                words.emplace_back();
                in.str(&words.back());
            }
            std::vector<const char *> args;
            for (const std::string &word : words) {
                args.push_back(word.c_str());
            }
            args.push_back(nullptr);
            rc = mpv_command(m_mpv, args.data());
            break;
        }
        case mpvwire::Op::SetPropertyString:
            in.str(&a);
            in.str(&b);
            rc = mpv_set_property_string(m_mpv, a.c_str(), b.c_str());
            break;
        case mpvwire::Op::GetProperty: {
            in.str(&a);
            const auto format = static_cast<mpv_format>(in.i32());
            readProperty(a.c_str(), format, reply);
            send(reply);
            return;
        }
        case mpvwire::Op::GetPropertyString: {
            in.str(&a);
            char *value = mpv_get_property_string(m_mpv, a.c_str());
            reply.i32(value ? 0 : MPV_ERROR_PROPERTY_UNAVAILABLE);
            reply.value(MPV_FORMAT_STRING, value ? &value : nullptr);
            if (value) {
                mpv_free(value);
            }
            send(reply);
            return;
        }
        case mpvwire::Op::SetWindow: {
            std::lock_guard<std::mutex> lock(g_surfaceLock);
            if (!g_surface) {
                note("the app asked for the video surface before handing one over");
                rc = MPV_ERROR_INVALID_PARAMETER;
                break;
            }
            int64_t wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(g_surface));
            rc = mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
            break;
        }
        default:
            break;
        }

        if (!in.ok()) {
            rc = MPV_ERROR_INVALID_PARAMETER;
        }
        reply.i32(rc);
        reply.value(MPV_FORMAT_NONE, nullptr);
        send(reply);
    }

    void readProperty(const char *name, mpv_format format, mpvwire::Writer &reply)
    {
        int rc = MPV_ERROR_INVALID_PARAMETER;
        switch (format) {
        case MPV_FORMAT_FLAG: {
            int value = 0;
            rc = mpv_get_property(m_mpv, name, format, &value);
            reply.i32(rc);
            reply.value(format, rc >= 0 ? &value : nullptr);
            return;
        }
        case MPV_FORMAT_INT64: {
            int64_t value = 0;
            rc = mpv_get_property(m_mpv, name, format, &value);
            reply.i32(rc);
            reply.value(format, rc >= 0 ? &value : nullptr);
            return;
        }
        case MPV_FORMAT_DOUBLE: {
            double value = 0.0;
            rc = mpv_get_property(m_mpv, name, format, &value);
            reply.i32(rc);
            reply.value(format, rc >= 0 ? &value : nullptr);
            return;
        }
        case MPV_FORMAT_NODE: {
            mpv_node value{};
            rc = mpv_get_property(m_mpv, name, format, &value);
            reply.i32(rc);
            reply.value(format, rc >= 0 ? &value : nullptr);
            if (rc >= 0) {
                mpv_free_node_contents(&value);
            }
            return;
        }
        default:
            reply.i32(rc);
            reply.value(MPV_FORMAT_NONE, nullptr);
            return;
        }
    }

    void forwardEvents()
    {
        while (true) {
            mpv_event *event = mpv_wait_event(m_mpv, -1);
            if (!event || event->event_id == MPV_EVENT_NONE) {
                continue;
            }
            mpvwire::Writer out;
            mpvwire::encodeEvent(out, event);
            if (out.bytes().size() > mpvwire::kMaxMessage) {
                continue;
            }
            send(out);
            if (event->event_id == MPV_EVENT_SHUTDOWN) {
                return;
            }
        }
    }

    const int m_fd;
    std::vector<char> m_buffer;
    mpv_handle *m_mpv = nullptr;
    bool m_eventsRunning = false;

    std::mutex m_sendLock;
    std::mutex m_queueLock;
    std::condition_variable m_queueReady;
    std::deque<Request> m_queue;
};

std::mutex g_sessionLock;
Session *g_session = nullptr;

}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *)
{
    g_vm = vm;
    handOverJavaVm();
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_topicdev_makimedia_org_MakimediaPlayerHost_nativeStart(JNIEnv *, jclass, jint fd)
{
    std::lock_guard<std::mutex> lock(g_sessionLock);
    if (g_session) {
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "a second player was asked of a process that has one");
        close(fd);
        return JNI_FALSE;
    }
    const int size = mpvwire::kSocketBuffer;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
    g_session = new Session(fd);
    g_session->start();
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_topicdev_makimedia_org_MakimediaPlayerHost_nativeSetSurface(JNIEnv *env, jclass,
                                                                    jobject surface)
{
    std::lock_guard<std::mutex> lock(g_surfaceLock);
    g_surface = nullptr;
    if (surface) {
        g_surface = env->NewGlobalRef(surface);
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "video surface %s",
                        surface ? "handed over" : "taken back");
}
