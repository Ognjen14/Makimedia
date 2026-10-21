#pragma once

#include <QObject>

class AppUpdates : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int state READ state NOTIFY stateChanged FINAL)

public:
    enum State {
        Idle = 0,
        Available = 1,
        Ready = 2
    };
    Q_ENUM(State)

    static AppUpdates *instance();

    int state() const;

    Q_INVOKABLE void check();
    Q_INVOKABLE void install();

    Q_INVOKABLE void markAvailable();
    Q_INVOKABLE void markReady();

signals:
    void stateChanged();

private:
    explicit AppUpdates(QObject *parent = nullptr);
    void setState(State state);

    State m_state = Idle;
    bool m_checking = false;
};
