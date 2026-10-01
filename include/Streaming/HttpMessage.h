#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>

struct HttpRequest
{
    QByteArray method;
    QString path;
    QByteArray query;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
    bool keepAlive = true;

    QByteArray header(const QByteArray &name) const
    {
        return headers.value(name.toLower());
    }
};

struct ByteRange
{
    bool requested = false;
    bool satisfiable = true;
    qint64 start = 0;
    qint64 end = -1;

    qint64 length() const { return end - start + 1; }
};

namespace HttpMessage {

enum class Parse {
    NeedMore,
    Ready,
    Bad
};

inline constexpr int MaxHeadBytes = 16 * 1024;
inline constexpr int MaxBodyBytes = 1024 * 1024;

Parse takeRequest(QByteArray &buffer, HttpRequest &request);

ByteRange parseRange(const QByteArray &header, qint64 size);

QByteArray reason(int status);

QByteArray head(int status, const QList<QPair<QByteArray, QByteArray>> &headers);

}
