#pragma once

#include <QObject>
#include <QString>

class IAudioFocus : public QObject
{
    Q_OBJECT

public:
    explicit IAudioFocus(QObject *parent = nullptr) : QObject(parent) {}
    ~IAudioFocus() override = default;

    virtual bool requestFocus() = 0;
    virtual void abandonFocus() = 0;
    virtual bool holdsFocus() const = 0;

    void simulate(const QString &event)
    {
        if (event == QLatin1String("loss")) {
            emit focusLost();
        } else if (event == QLatin1String("transient")) {
            emit focusLostTransient();
        } else if (event == QLatin1String("regain")) {
            emit focusRegained();
        } else if (event == QLatin1String("duck")) {
            emit shouldDuck(true);
        } else if (event == QLatin1String("unduck")) {
            emit shouldDuck(false);
        } else if (event == QLatin1String("noisy")) {
            emit becomingNoisy();
        }
    }

signals:
    void focusLost();
    void focusLostTransient();
    void focusRegained();
    void shouldDuck(bool duck);
    void becomingNoisy();
};
