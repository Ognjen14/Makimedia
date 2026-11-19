#pragma once

#include <QByteArray>
#include <QVariant>

#include <cstdint>

#include <mpv/client.h>

class MpvEngine
{
public:
    virtual ~MpvEngine() = default;

    virtual mpv_handle *rawHandle() const { return nullptr; }
    virtual bool remote() const { return false; }

    virtual void abandon() {}

    virtual bool alive() const { return true; }

    virtual int setOptionString(const char *name, const char *value) = 0;
    virtual int initialize() = 0;
    virtual int requestLogMessages(const char *level) = 0;
    virtual int observeProperty(uint64_t userdata, const char *name, mpv_format format) = 0;
    virtual void setWakeupCallback(void (*callback)(void *), void *context) = 0;
    virtual int hookAdd(uint64_t userdata, const char *name, int priority) = 0;
    virtual int hookContinue(uint64_t id) = 0;
    virtual int command(const char **args) = 0;
    virtual int setPropertyString(const char *name, const char *value) = 0;
    virtual int getDouble(const char *name, double *out) = 0;
    virtual int getInt64(const char *name, int64_t *out) = 0;
    virtual bool getString(const char *name, QByteArray *out) = 0;
    virtual bool getVariant(const char *name, QVariant *out) = 0;

    virtual int setWindow(qint64 windowHandle) = 0;

    virtual mpv_event *waitEvent(double timeout) = 0;
};

QVariant mpvNodeToVariant(const mpv_node *node);

class LocalMpvEngine final : public MpvEngine
{
public:
    static LocalMpvEngine *create();
    ~LocalMpvEngine() override;

    mpv_handle *rawHandle() const override { return m_mpv; }

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
    explicit LocalMpvEngine(mpv_handle *mpv) : m_mpv(mpv) {}

    mpv_handle *m_mpv = nullptr;
    bool m_initialized = false;
};
