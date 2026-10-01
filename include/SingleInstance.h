#pragma once

#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    bool isPrimary() const;
    bool handOver(const QString &path);

signals:
    void openRequested(const QString &path);

private:
    void onNewConnection();

    QString m_key;
    QLocalServer *m_server = nullptr;
    QLocalSocket *m_clientSocket = nullptr;
    bool m_primary = true;
};
