#pragma once

#include <QString>
#include <QtGlobal>

namespace StreamProtocol {

inline constexpr quint16 DiscoveryPort = 47810;
inline constexpr quint16 HttpPort = 47811;
inline constexpr int Version = 1;
inline constexpr int DiscoveryIntervalMs = 2000;

inline constexpr int HeartbeatIntervalMs = 5000;
inline constexpr int HeartbeatTimeoutMs = 4000;
inline constexpr int HeartbeatMissesAllowed = 3;
inline constexpr qint64 DeviceSilenceMs = 15000;

inline QString tokenPrefix(const QString &token)
{
    return QStringLiteral("/t/") + token;
}

inline QString withoutToken(const QString &text)
{
    const QString marker = QStringLiteral("/t/");
    QString out = text;
    qsizetype from = 0;
    while (true) {
        const qsizetype start = out.indexOf(marker, from);
        if (start < 0) {
            return out;
        }
        const qsizetype tokenStart = start + marker.size();
        qsizetype tokenEnd = out.indexOf(QLatin1Char('/'), tokenStart);
        if (tokenEnd < 0) {
            tokenEnd = out.size();
        }
        out.replace(tokenStart, tokenEnd - tokenStart, QStringLiteral("-"));
        from = tokenStart + 1;
    }
}

}
