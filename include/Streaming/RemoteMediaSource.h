#pragma once

#include "Platform/IMediaSource.h"

#include <QString>

#include <functional>

class RemoteMediaSource : public IMediaSource
{
public:
    using IdResolver = std::function<qint64(const QString &handle)>;

    RemoteMediaSource(const QString &host, quint16 port, const QString &token = QString());

    void setIdResolver(IdResolver resolver);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    QString token() const { return m_token; }

    QString baseUrl() const;
    QString sessionUrl() const;
    QString urlForFileId(qint64 fileId) const;

    QString mpvUrl(const QString &handle) override;
    bool exists(const QString &handle) const override;
    MediaFileInfo info(const QString &handle) const override;
    QString displayPath(const QString &handle) const override;
    QStringList siblingSubtitles(const QString &handle) const override;
    QList<MediaFileInfo> listChildren(const QString &handle) const override;
    QString parentOf(const QString &handle) const override;
    bool isRootAvailable(const QString &rootHandle) const override;

private:
    qint64 idFor(const QString &handle) const;
    QByteArray fetch(const QString &path, int *status) const;

    QString m_host;
    quint16 m_port = 0;
    QString m_token;
    IdResolver m_resolver;
};
