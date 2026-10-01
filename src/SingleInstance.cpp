#include "SingleInstance.h"

#include "MmLog.h"

#include <QDataStream>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

constexpr int kHandOverWaitMs = 1000;
constexpr qsizetype kLengthBytes = 4;

}

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
    m_clientSocket = new QLocalSocket(this);
    m_clientSocket->connectToServer(m_key);

    if (m_clientSocket->waitForConnected(300)) {
        m_primary = false;
        MM_LOG_I() << "another instance already owns" << m_key
                   << "- handing over to it";
        return;
    }

    const QLocalSocket::LocalSocketError failure = m_clientSocket->error();
    m_clientSocket->abort();
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;

    if (failure == QLocalSocket::SocketTimeoutError) {
        MM_LOG_W() << "the running instance did not answer in time,"
                   << "leaving its socket alone";
    } else {
        QLocalServer::removeServer(m_key);
    }

    m_server = new QLocalServer(this);
    if (!m_server->listen(m_key)) {
        MM_LOG_W() << "could not claim the single instance socket"
                   << m_server->errorString()
                   << "- a second window may open instead of handing over";
        return;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &SingleInstance::onNewConnection);
    MM_LOG_I() << "single instance server listening on" << m_key;
}

SingleInstance::~SingleInstance()
{
    if (m_server) {
        m_server->close();
    }
}

bool SingleInstance::isPrimary() const
{
    return m_primary;
}

bool SingleInstance::handOver(const QString &path)
{
    if (m_primary || !m_clientSocket) {
        return false;
    }

    const QByteArray payload = path.toUtf8();
    QByteArray message;
    {
        QDataStream out(&message, QIODevice::WriteOnly);
        out << quint32(payload.size());
    }
    message.append(payload);

    m_clientSocket->write(message);
    while (m_clientSocket->bytesToWrite() > 0) {
        if (!m_clientSocket->waitForBytesWritten(kHandOverWaitMs)) {
            MM_LOG_W() << "hand over to the running instance timed out with"
                       << m_clientSocket->bytesToWrite() << "bytes unsent";
            return false;
        }
    }

    m_clientSocket->disconnectFromServer();
    MM_LOG_I() << "handed over to the running instance"
               << (path.isEmpty() ? QStringLiteral("with no file") : path);
    return true;
}

void SingleInstance::onNewConnection()
{
    QLocalSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QLocalSocket::disconnected,
            socket, &QLocalSocket::deleteLater);

    QByteArray received;
    quint32 expected = 0;
    bool haveLength = false;

    QElapsedTimer waited;
    waited.start();

    while (true) {
        received += socket->readAll();

        if (!haveLength && received.size() >= kLengthBytes) {
            QDataStream in(received.left(kLengthBytes));
            in >> expected;
            haveLength = true;
        }
        if (haveLength && received.size() >= kLengthBytes + qsizetype(expected)) {
            break;
        }

        const int left = kHandOverWaitMs - int(waited.elapsed());
        if (left <= 0 || !socket->waitForReadyRead(left)) {
            received += socket->readAll();
            break;
        }
    }

    QString payload;
    if (haveLength && received.size() >= kLengthBytes + qsizetype(expected)) {
        payload = QString::fromUtf8(received.mid(kLengthBytes, qsizetype(expected)));
    } else {
        MM_LOG_W() << "a second instance's hand over arrived incomplete,"
                   << received.size() << "bytes";
    }

    MM_LOG_I() << "second instance handed over"
               << (payload.isEmpty() ? QStringLiteral("with no file") : payload);

    socket->disconnectFromServer();
    emit openRequested(payload);
}
