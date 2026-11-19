#pragma once

#include <mpv/client.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace mpvwire {

constexpr size_t kMaxMessage = 4 * 1024 * 1024;
constexpr int kSocketBuffer = 4 * 1024 * 1024;

enum class Kind : uint32_t {
    Request = 1,
    Reply = 2,
    Event = 3,
};

enum class Op : uint32_t {
    SetOptionString = 1,
    Initialize,
    RequestLogMessages,
    ObserveProperty,
    HookAdd,
    HookContinue,
    Command,
    SetPropertyString,
    GetProperty,
    GetPropertyString,
    SetWindow,
    Quit,
};

class Writer
{
public:
    void u8(uint8_t v) { raw(&v, sizeof v); }
    void u32(uint32_t v) { raw(&v, sizeof v); }
    void i32(int32_t v) { raw(&v, sizeof v); }
    void u64(uint64_t v) { raw(&v, sizeof v); }
    void i64(int64_t v) { raw(&v, sizeof v); }
    void f64(double v) { raw(&v, sizeof v); }

    void str(const char *s)
    {
        if (!s) {
            u8(0);
            return;
        }
        u8(1);
        const uint32_t length = static_cast<uint32_t>(std::strlen(s));
        u32(length);
        raw(s, length);
    }

    void node(const mpv_node &n, int depth = 0)
    {
        if (depth > 64) {
            i32(MPV_FORMAT_NONE);
            return;
        }
        switch (n.format) {
        case MPV_FORMAT_STRING:
            i32(MPV_FORMAT_STRING);
            str(n.u.string);
            break;
        case MPV_FORMAT_FLAG:
            i32(MPV_FORMAT_FLAG);
            i32(n.u.flag);
            break;
        case MPV_FORMAT_INT64:
            i32(MPV_FORMAT_INT64);
            i64(n.u.int64);
            break;
        case MPV_FORMAT_DOUBLE:
            i32(MPV_FORMAT_DOUBLE);
            f64(n.u.double_);
            break;
        case MPV_FORMAT_NODE_ARRAY:
        case MPV_FORMAT_NODE_MAP: {
            const bool map = n.format == MPV_FORMAT_NODE_MAP;
            i32(n.format);
            const mpv_node_list *list = n.u.list;
            const int count = list ? list->num : 0;
            u32(static_cast<uint32_t>(count));
            for (int i = 0; i < count; ++i) {
                if (map) {
                    str(list->keys[i]);
                }
                node(list->values[i], depth + 1);
            }
            break;
        }
        default:
            i32(MPV_FORMAT_NONE);
            break;
        }
    }

    void value(mpv_format format, const void *data)
    {
        if (!data || format == MPV_FORMAT_NONE) {
            u8(0);
            return;
        }
        u8(1);
        i32(format);
        switch (format) {
        case MPV_FORMAT_FLAG:
            i32(*static_cast<const int *>(data));
            break;
        case MPV_FORMAT_INT64:
            i64(*static_cast<const int64_t *>(data));
            break;
        case MPV_FORMAT_DOUBLE:
            f64(*static_cast<const double *>(data));
            break;
        case MPV_FORMAT_STRING:
        case MPV_FORMAT_OSD_STRING:
            str(*static_cast<char *const *>(data));
            break;
        case MPV_FORMAT_NODE:
            node(*static_cast<const mpv_node *>(data));
            break;
        default:
            break;
        }
    }

    const std::string &bytes() const { return m_bytes; }

private:
    void raw(const void *p, size_t n) { m_bytes.append(static_cast<const char *>(p), n); }

    std::string m_bytes;
};

class Reader
{
public:
    Reader(const char *data, size_t size) : m_p(data), m_end(data + size) {}

    bool ok() const { return m_ok; }

    uint8_t u8() { return pod<uint8_t>(); }
    uint32_t u32() { return pod<uint32_t>(); }
    int32_t i32() { return pod<int32_t>(); }
    uint64_t u64() { return pod<uint64_t>(); }
    int64_t i64() { return pod<int64_t>(); }
    double f64() { return pod<double>(); }

    bool str(std::string *out)
    {
        out->clear();
        if (u8() == 0) {
            return false;
        }
        const uint32_t length = u32();
        if (!m_ok || static_cast<size_t>(m_end - m_p) < length) {
            m_ok = false;
            return false;
        }
        out->assign(m_p, length);
        m_p += length;
        return true;
    }

private:
    template <class T>
    T pod()
    {
        T v{};
        if (!m_ok || static_cast<size_t>(m_end - m_p) < sizeof(T)) {
            m_ok = false;
            return v;
        }
        std::memcpy(&v, m_p, sizeof(T));
        m_p += sizeof(T);
        return v;
    }

    const char *m_p;
    const char *m_end;
    bool m_ok = true;
};

struct Arena
{
    std::deque<std::string> strings;
    std::deque<std::vector<mpv_node>> nodes;
    std::deque<std::vector<char *>> keys;
    std::deque<mpv_node_list> lists;

    char *keep(std::string s)
    {
        strings.push_back(std::move(s));
        return &strings.back()[0];
    }

    void clear()
    {
        strings.clear();
        nodes.clear();
        keys.clear();
        lists.clear();
    }
};

inline bool decodeNode(Reader &in, Arena &arena, mpv_node *out, int depth = 0)
{
    out->format = MPV_FORMAT_NONE;
    if (depth > 64) {
        return false;
    }
    const auto format = static_cast<mpv_format>(in.i32());
    switch (format) {
    case MPV_FORMAT_STRING: {
        std::string text;
        const bool present = in.str(&text);
        out->format = MPV_FORMAT_STRING;
        out->u.string = present ? arena.keep(std::move(text)) : arena.keep(std::string());
        break;
    }
    case MPV_FORMAT_FLAG:
        out->format = MPV_FORMAT_FLAG;
        out->u.flag = in.i32();
        break;
    case MPV_FORMAT_INT64:
        out->format = MPV_FORMAT_INT64;
        out->u.int64 = in.i64();
        break;
    case MPV_FORMAT_DOUBLE:
        out->format = MPV_FORMAT_DOUBLE;
        out->u.double_ = in.f64();
        break;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        const bool map = format == MPV_FORMAT_NODE_MAP;
        const uint32_t count = in.u32();
        if (!in.ok() || count > 1000000) {
            return false;
        }
        arena.nodes.emplace_back(count);
        std::vector<mpv_node> &values = arena.nodes.back();
        char **keys = nullptr;
        if (map) {
            arena.keys.emplace_back(count, nullptr);
            keys = arena.keys.back().data();
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (map) {
                std::string key;
                in.str(&key);
                keys[i] = arena.keep(std::move(key));
            }
            if (!decodeNode(in, arena, &values[i], depth + 1)) {
                return false;
            }
        }
        arena.lists.push_back(mpv_node_list{});
        mpv_node_list &list = arena.lists.back();
        list.num = static_cast<int>(count);
        list.values = values.data();
        list.keys = keys;
        out->format = format;
        out->u.list = &list;
        break;
    }
    default:
        break;
    }
    return in.ok();
}

struct Value
{
    bool present = false;
    mpv_format format = MPV_FORMAT_NONE;
    int flag = 0;
    int64_t int64 = 0;
    double double_ = 0.0;
    char *string = nullptr;
    mpv_node node{};

    void *data()
    {
        if (!present) {
            return nullptr;
        }
        switch (format) {
        case MPV_FORMAT_FLAG:
            return &flag;
        case MPV_FORMAT_INT64:
            return &int64;
        case MPV_FORMAT_DOUBLE:
            return &double_;
        case MPV_FORMAT_STRING:
        case MPV_FORMAT_OSD_STRING:
            return &string;
        case MPV_FORMAT_NODE:
            return &node;
        default:
            return nullptr;
        }
    }
};

inline bool decodeValue(Reader &in, Arena &arena, Value *out)
{
    *out = Value{};
    if (in.u8() == 0) {
        return in.ok();
    }
    out->present = true;
    out->format = static_cast<mpv_format>(in.i32());
    switch (out->format) {
    case MPV_FORMAT_FLAG:
        out->flag = in.i32();
        break;
    case MPV_FORMAT_INT64:
        out->int64 = in.i64();
        break;
    case MPV_FORMAT_DOUBLE:
        out->double_ = in.f64();
        break;
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING: {
        std::string text;
        if (in.str(&text)) {
            out->string = arena.keep(std::move(text));
        } else {
            out->present = false;
        }
        break;
    }
    case MPV_FORMAT_NODE:
        return decodeNode(in, arena, &out->node);
    default:
        out->present = false;
        break;
    }
    return in.ok();
}

inline void encodeEvent(Writer &out, const mpv_event *event)
{
    out.u32(static_cast<uint32_t>(Kind::Event));
    out.u32(static_cast<uint32_t>(event->event_id));
    out.i32(event->error);
    out.u64(event->reply_userdata);

    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
    case MPV_EVENT_GET_PROPERTY_REPLY: {
        const auto *property = static_cast<const mpv_event_property *>(event->data);
        out.str(property ? property->name : "");
        out.value(property ? property->format : MPV_FORMAT_NONE,
                  property ? property->data : nullptr);
        break;
    }
    case MPV_EVENT_LOG_MESSAGE: {
        const auto *message = static_cast<const mpv_event_log_message *>(event->data);
        out.str(message ? message->prefix : "");
        out.str(message ? message->level : "");
        out.str(message ? message->text : "");
        out.i32(message ? message->log_level : MPV_LOG_LEVEL_INFO);
        break;
    }
    case MPV_EVENT_HOOK: {
        const auto *hook = static_cast<const mpv_event_hook *>(event->data);
        out.str(hook ? hook->name : "");
        out.u64(hook ? hook->id : 0);
        break;
    }
    case MPV_EVENT_END_FILE: {
        const auto *endFile = static_cast<const mpv_event_end_file *>(event->data);
        out.i32(endFile ? static_cast<int32_t>(endFile->reason) : 0);
        out.i32(endFile ? endFile->error : 0);
        out.i64(endFile ? endFile->playlist_entry_id : 0);
        break;
    }
    case MPV_EVENT_START_FILE: {
        const auto *startFile = static_cast<const mpv_event_start_file *>(event->data);
        out.i64(startFile ? startFile->playlist_entry_id : 0);
        break;
    }
    default:
        break;
    }
}

struct DecodedEvent
{
    mpv_event event{};
    Arena arena;
    Value value;
    mpv_event_property property{};
    mpv_event_log_message log{};
    mpv_event_hook hook{};
    mpv_event_end_file endFile{};
    mpv_event_start_file startFile{};
};

inline bool decodeEvent(Reader &in, DecodedEvent *out)
{
    out->event.event_id = static_cast<mpv_event_id>(in.u32());
    out->event.error = in.i32();
    out->event.reply_userdata = in.u64();
    out->event.data = nullptr;

    std::string text;
    switch (out->event.event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
    case MPV_EVENT_GET_PROPERTY_REPLY:
        in.str(&text);
        out->property.name = out->arena.keep(std::move(text));
        if (!decodeValue(in, out->arena, &out->value)) {
            return false;
        }
        out->property.format = out->value.present ? out->value.format : MPV_FORMAT_NONE;
        out->property.data = out->value.data();
        out->event.data = &out->property;
        break;
    case MPV_EVENT_LOG_MESSAGE:
        in.str(&text);
        out->log.prefix = out->arena.keep(std::move(text));
        in.str(&text);
        out->log.level = out->arena.keep(std::move(text));
        in.str(&text);
        out->log.text = out->arena.keep(std::move(text));
        out->log.log_level = static_cast<mpv_log_level>(in.i32());
        out->event.data = &out->log;
        break;
    case MPV_EVENT_HOOK:
        in.str(&text);
        out->hook.name = out->arena.keep(std::move(text));
        out->hook.id = in.u64();
        out->event.data = &out->hook;
        break;
    case MPV_EVENT_END_FILE:
        out->endFile.reason = static_cast<mpv_end_file_reason>(in.i32());
        out->endFile.error = in.i32();
        out->endFile.playlist_entry_id = in.i64();
        out->event.data = &out->endFile;
        break;
    case MPV_EVENT_START_FILE:
        out->startFile.playlist_entry_id = in.i64();
        out->event.data = &out->startFile;
        break;
    default:
        break;
    }
    return in.ok();
}

}
