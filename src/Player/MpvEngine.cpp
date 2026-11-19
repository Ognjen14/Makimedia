#include "Player/MpvEngine.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

QVariant mpvNodeToVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0;
    case MPV_FORMAT_INT64:
        return static_cast<qint64>(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        const mpv_node_list *entries = node->u.list;
        for (int i = 0; entries && i < entries->num; ++i) {
            list.append(mpvNodeToVariant(&entries->values[i]));
        }
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        const mpv_node_list *entries = node->u.list;
        for (int i = 0; entries && i < entries->num; ++i) {
            map.insert(QString::fromUtf8(entries->keys[i]),
                       mpvNodeToVariant(&entries->values[i]));
        }
        return map;
    }
    default:
        return QVariant();
    }
}

LocalMpvEngine *LocalMpvEngine::create()
{
    mpv_handle *mpv = mpv_create();
    return mpv ? new LocalMpvEngine(mpv) : nullptr;
}

LocalMpvEngine::~LocalMpvEngine()
{
    if (!m_mpv) {
        return;
    }
    if (m_initialized) {
        mpv_terminate_destroy(m_mpv);
    } else {
        mpv_destroy(m_mpv);
    }
}

int LocalMpvEngine::setOptionString(const char *name, const char *value)
{
    return mpv_set_option_string(m_mpv, name, value);
}

int LocalMpvEngine::initialize()
{
    const int rc = mpv_initialize(m_mpv);
    m_initialized = rc >= 0;
    return rc;
}

int LocalMpvEngine::requestLogMessages(const char *level)
{
    return mpv_request_log_messages(m_mpv, level);
}

int LocalMpvEngine::observeProperty(uint64_t userdata, const char *name, mpv_format format)
{
    return mpv_observe_property(m_mpv, userdata, name, format);
}

void LocalMpvEngine::setWakeupCallback(void (*callback)(void *), void *context)
{
    mpv_set_wakeup_callback(m_mpv, callback, context);
}

int LocalMpvEngine::hookAdd(uint64_t userdata, const char *name, int priority)
{
    return mpv_hook_add(m_mpv, userdata, name, priority);
}

int LocalMpvEngine::hookContinue(uint64_t id)
{
    return mpv_hook_continue(m_mpv, id);
}

int LocalMpvEngine::command(const char **args)
{
    return mpv_command(m_mpv, args);
}

int LocalMpvEngine::setPropertyString(const char *name, const char *value)
{
    return mpv_set_property_string(m_mpv, name, value);
}

int LocalMpvEngine::getDouble(const char *name, double *out)
{
    return mpv_get_property(m_mpv, name, MPV_FORMAT_DOUBLE, out);
}

int LocalMpvEngine::getInt64(const char *name, int64_t *out)
{
    return mpv_get_property(m_mpv, name, MPV_FORMAT_INT64, out);
}

bool LocalMpvEngine::getString(const char *name, QByteArray *out)
{
    char *value = mpv_get_property_string(m_mpv, name);
    if (!value) {
        return false;
    }
    *out = QByteArray(value);
    mpv_free(value);
    return true;
}

bool LocalMpvEngine::getVariant(const char *name, QVariant *out)
{
    mpv_node node;
    if (mpv_get_property(m_mpv, name, MPV_FORMAT_NODE, &node) < 0) {
        return false;
    }
    *out = mpvNodeToVariant(&node);
    mpv_free_node_contents(&node);
    return true;
}

int LocalMpvEngine::setWindow(qint64 windowHandle)
{
    int64_t wid = windowHandle;
    return mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
}

mpv_event *LocalMpvEngine::waitEvent(double timeout)
{
    return mpv_wait_event(m_mpv, timeout);
}
