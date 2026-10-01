#include "Streaming/Discovery.h"

#include "MmLog.h"
#include "Streaming/AddressFilter.h"
#include "Streaming/StreamProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>

namespace Discovery {

namespace {

QJsonObject readObject(const QByteArray &datagram)
{
    if (datagram.size() > 1024) {
        return QJsonObject();
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(datagram, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return QJsonObject();
    }
    return document.object();
}

}

QByteArray askMessage()
{
    const QJsonObject ask{
        { QStringLiteral("mm"), QStringLiteral("discover") },
        { QStringLiteral("v"), StreamProtocol::Version }
    };
    return QJsonDocument(ask).toJson(QJsonDocument::Compact);
}

bool isAsk(const QByteArray &datagram)
{
    const QJsonObject object = readObject(datagram);
    return object.value(QStringLiteral("mm")).toString() == QLatin1String("discover")
           && object.value(QStringLiteral("v")).toInt() == StreamProtocol::Version;
}

QByteArray hereMessage(const Here &here)
{
    const QJsonObject message{
        { QStringLiteral("mm"), QStringLiteral("here") },
        { QStringLiteral("v"), StreamProtocol::Version },
        { QStringLiteral("id"), here.serverId },
        { QStringLiteral("name"), here.name },
        { QStringLiteral("port"), int(here.port) }
    };
    return QJsonDocument(message).toJson(QJsonDocument::Compact);
}

std::optional<Here> parseHere(const QByteArray &datagram)
{
    const QJsonObject object = readObject(datagram);
    if (object.value(QStringLiteral("mm")).toString() != QLatin1String("here")
        || object.value(QStringLiteral("v")).toInt() != StreamProtocol::Version) {
        return std::nullopt;
    }

    Here here;
    here.serverId = object.value(QStringLiteral("id")).toString();
    here.name = object.value(QStringLiteral("name")).toString();
    const int port = object.value(QStringLiteral("port")).toInt();
    if (here.serverId.isEmpty() || port <= 0 || port > 65535) {
        return std::nullopt;
    }
    here.port = quint16(port);
    return here;
}

}

DiscoveryResponder::DiscoveryResponder(QObject *parent)
    : QObject(parent)
{
}

DiscoveryResponder::~DiscoveryResponder()
{
    stop();
}

bool DiscoveryResponder::start(const Discovery::Here &here, quint16 udpPort)
{
    stop();
    m_here = here;
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, udpPort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        MM_LOG_E() << "discovery: could not listen on UDP" << udpPort << "-"
                   << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &DiscoveryResponder::answer);
    MM_LOG_I() << "discovery: answering on UDP" << udpPort << "as" << here.name;
    return true;
}

void DiscoveryResponder::stop()
{
    if (!m_socket) {
        return;
    }
    m_socket->close();
    delete m_socket;
    m_socket = nullptr;
    m_answered.clear();
    MM_LOG_I() << "discovery: stopped answering";
}

bool DiscoveryResponder::isRunning() const
{
    return m_socket != nullptr;
}

void DiscoveryResponder::answer()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        if (!Discovery::isAsk(datagram.data())) {
            continue;
        }
        const QHostAddress sender = datagram.senderAddress();
        if (!AddressFilter::isPrivatePeer(sender)) {
            MM_LOG_W() << "discovery: ignored an ask from" << AddressFilter::readable(sender);
            continue;
        }
        m_socket->writeDatagram(Discovery::hereMessage(m_here), sender,
                                quint16(datagram.senderPort()));
        const QString peer = AddressFilter::readable(sender);
        if (!m_answered.contains(peer)) {
            m_answered.insert(peer);
            MM_LOG_I() << "discovery: a device at" << peer << "is looking, and was answered";
        }
    }
}

DiscoveryBrowser::DiscoveryBrowser(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &DiscoveryBrowser::ask);
}

DiscoveryBrowser::~DiscoveryBrowser()
{
    stop();
}

void DiscoveryBrowser::setExtraTargets(const QList<QHostAddress> &targets)
{
    m_extraTargets = targets;
}

bool DiscoveryBrowser::start(quint16 udpPort, int intervalMs)
{
    stop();
    m_udpPort = udpPort;
    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, 0)) {
        MM_LOG_E() << "discovery: could not open a socket to look with -"
                   << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return false;
    }
    connect(m_socket, &QUdpSocket::readyRead, this, &DiscoveryBrowser::read);
    m_timer.start(intervalMs);
    MM_LOG_I() << "discovery: looking for a streaming PC on UDP" << udpPort;
    ask();
    return true;
}

void DiscoveryBrowser::stop()
{
    m_timer.stop();
    if (!m_socket) {
        return;
    }
    m_socket->close();
    delete m_socket;
    m_socket = nullptr;
    MM_LOG_I() << "discovery: stopped looking";
}

bool DiscoveryBrowser::isRunning() const
{
    return m_socket != nullptr;
}

void DiscoveryBrowser::ask()
{
    if (!m_socket) {
        return;
    }

    const QByteArray message = Discovery::askMessage();
    QList<QHostAddress> targets = m_extraTargets;
    targets.append(QHostAddress(QHostAddress::Broadcast));

    const QList<QNetworkInterface> adapters = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &adapter : adapters) {
        const auto flags = adapter.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::CanBroadcast)
            || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = adapter.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            const QHostAddress broadcast = entry.broadcast();
            if (!broadcast.isNull() && !targets.contains(broadcast)) {
                targets.append(broadcast);
            }
        }
    }

    for (const QHostAddress &target : targets) {
        m_socket->writeDatagram(message, target, m_udpPort);
    }
}

void DiscoveryBrowser::read()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        const std::optional<Discovery::Here> here = Discovery::parseHere(datagram.data());
        if (!here) {
            continue;
        }
        emit found(here->serverId, here->name,
                   AddressFilter::readable(datagram.senderAddress()), here->port);
    }
}
