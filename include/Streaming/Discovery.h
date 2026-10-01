#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <optional>

class QUdpSocket;

namespace Discovery {

struct Here
{
    QString serverId;
    QString name;
    quint16 port = 0;
};

QByteArray askMessage();
bool isAsk(const QByteArray &datagram);

QByteArray hereMessage(const Here &here);
std::optional<Here> parseHere(const QByteArray &datagram);

}

class DiscoveryResponder : public QObject
{
    Q_OBJECT

public:
    explicit DiscoveryResponder(QObject *parent = nullptr);
    ~DiscoveryResponder() override;

    bool start(const Discovery::Here &here, quint16 udpPort);
    void stop();
    bool isRunning() const;

private:
    void answer();

    QUdpSocket *m_socket = nullptr;
    Discovery::Here m_here;
    QSet<QString> m_answered;
};

class DiscoveryBrowser : public QObject
{
    Q_OBJECT

public:
    explicit DiscoveryBrowser(QObject *parent = nullptr);
    ~DiscoveryBrowser() override;

    void setExtraTargets(const QList<QHostAddress> &targets);
    bool start(quint16 udpPort, int intervalMs);
    void stop();
    bool isRunning() const;

signals:
    void found(const QString &serverId, const QString &name,
               const QString &host, quint16 port);

private:
    void ask();
    void read();

    QUdpSocket *m_socket = nullptr;
    QTimer m_timer;
    quint16 m_udpPort = 0;
    QList<QHostAddress> m_extraTargets;
};
