#include "Streaming/MirrorSync.h"

#include "Data/Database.h"
#include "MmLog.h"
#include "Streaming/LibrarySnapshot.h"
#include "Streaming/Mirror.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

constexpr int kInfoTimeoutMs = 8000;
constexpr int kSnapshotTimeoutMs = 30000;

}

MirrorSync::MirrorSync(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_root(Mirror::defaultRoot())
{
}

MirrorSync::~MirrorSync()
{
    cancel();
}

void MirrorSync::setRoot(const QString &root)
{
    m_root = root;
}

bool MirrorSync::running() const
{
    return !m_reply.isNull();
}

QString MirrorSync::tempPath() const
{
    return Mirror::libraryPath(m_root, m_serverId) + QStringLiteral(".download");
}

void MirrorSync::start(const QString &baseUrl, const QString &serverId,
                       const QString &serverName)
{
    cancel();
    m_base = baseUrl;
    m_serverId = serverId;
    m_serverName = serverName;

    if (Mirror::directoryFor(m_root, m_serverId).isEmpty()) {
        fail(tr("The PC sent an id Makimedia cannot use"));
        return;
    }

    QNetworkRequest request(QUrl(m_base + QStringLiteral("/v1/library")));
    request.setTransferTimeout(kInfoTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &MirrorSync::onLibraryInfo);
    MM_LOG_I() << "mirror: asking" << m_serverName << "which library it has";
}

void MirrorSync::cancel()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_temp.isOpen()) {
        m_temp.close();
        Mirror::removeWithSidecars(m_temp.fileName());
        MM_LOG_I() << "mirror: download stopped, the stored library is untouched";
    }
}

void MirrorSync::fail(const QString &reason, bool newerSchema)
{
    MM_LOG_W() << "mirror:" << reason;
    emit failed(reason, newerSchema);
}

void MirrorSync::onLibraryInfo()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        fail(tr("%1 did not answer").arg(m_serverName));
        return;
    }

    const QJsonObject info = QJsonDocument::fromJson(reply->readAll()).object();
    m_revision = info.value(QStringLiteral("revision")).toString();
    m_bytes = info.value(QStringLiteral("bytes")).toInteger();
    m_schema = info.value(QStringLiteral("schema")).toInt();
    if (m_revision.isEmpty() || m_bytes <= 0) {
        fail(tr("%1 has no library to send yet").arg(m_serverName));
        return;
    }

    if (m_pretendNewer) {
        MM_LOG_W() << "mirror: developer switch - treating the library as newer than this app";
        m_schema = Database::targetSchemaVersion() + 1;
    }
    if (m_schema > Database::targetSchemaVersion()) {
        fail(tr("Update Makimedia on this device to watch from %1").arg(m_serverName), true);
        return;
    }

    const QString path = Mirror::libraryPath(m_root, m_serverId);
    const Mirror::Stored stored = Mirror::readInfo(m_root, m_serverId);
    if (stored.revision == m_revision && Mirror::looksLikeSqlite(path)) {
        MM_LOG_I() << "mirror: the library of" << m_serverName << "is unchanged,"
                   << "revision" << m_revision.left(12);
        emit ready(path, false);
        return;
    }

    const bool firstTime = !QFile::exists(path);
    MM_LOG_I() << "mirror:" << (firstTime ? "getting" : "updating") << "the library of"
               << m_serverName << "-" << m_bytes << "bytes, revision" << m_revision.left(12);

    QDir().mkpath(Mirror::directoryFor(m_root, m_serverId));
    Mirror::removeWithSidecars(tempPath());
    m_temp.setFileName(tempPath());
    if (!m_temp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(tr("There is no room to keep the library of %1").arg(m_serverName));
        return;
    }

    emit downloading(firstTime);
    emit progress(0, m_bytes);

    QNetworkRequest request(QUrl(m_base + QStringLiteral("/v1/library/snapshot")));
    request.setTransferTimeout(kSnapshotTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &MirrorSync::onSnapshotData);
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        emit progress(received, total > 0 ? total : m_bytes);
    });
    connect(m_reply, &QNetworkReply::finished, this, &MirrorSync::onSnapshotDone);
}

void MirrorSync::onSnapshotData()
{
    if (m_reply && m_temp.isOpen()) {
        m_temp.write(m_reply->readAll());
    }
}

void MirrorSync::onSnapshotDone()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (m_temp.isOpen()) {
        m_temp.write(reply->readAll());
        m_temp.close();
    }

    const QString temp = tempPath();
    if (reply->error() != QNetworkReply::NoError) {
        Mirror::removeWithSidecars(temp);
        fail(tr("The library of %1 stopped arriving").arg(m_serverName));
        return;
    }

    if (QFileInfo(temp).size() != m_bytes || !Mirror::looksLikeSqlite(temp)
        || LibrarySnapshot::revisionOf(temp) != m_revision) {
        Mirror::removeWithSidecars(temp);
        fail(tr("The library of %1 arrived damaged").arg(m_serverName));
        return;
    }

    const QString path = Mirror::libraryPath(m_root, m_serverId);
    int carried = 0;
    QString error;
    if (!Mirror::carryPlayback(path, temp, &carried, &error)) {
        Mirror::removeWithSidecars(temp);
        fail(error);
        return;
    }

    Mirror::removeWithSidecars(path);
    if (!QFile::rename(temp, path)) {
        Mirror::removeWithSidecars(temp);
        fail(tr("The library of %1 could not be kept").arg(m_serverName));
        return;
    }

    Mirror::Stored stored;
    stored.serverId = m_serverId;
    stored.name = m_serverName;
    stored.revision = m_revision;
    stored.bytes = m_bytes;
    stored.schemaVersion = m_schema;
    Mirror::writeInfo(m_root, stored);

    MM_LOG_I() << "mirror: the library of" << m_serverName << "is here,"
               << carried << "files kept their progress";
    emit ready(path, true);
}
