#include "Streaming/AddressFilter.h"

namespace {

QHostAddress unmapped(const QHostAddress &address)
{
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4 && address.protocol() == QAbstractSocket::IPv6Protocol) {
        return QHostAddress(v4);
    }
    return address;
}

}

namespace AddressFilter {

bool isPrivatePeer(const QHostAddress &address)
{
    if (address.isNull()) {
        return false;
    }

    const QHostAddress peer = unmapped(address);
    if (peer.isLoopback() || peer.isLinkLocal()) {
        return true;
    }

    if (peer.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = peer.toIPv4Address();
        const quint8 first = quint8(ip >> 24);
        const quint8 second = quint8((ip >> 16) & 0xff);
        if (first == 10) {
            return true;
        }
        if (first == 172 && second >= 16 && second <= 31) {
            return true;
        }
        if (first == 192 && second == 168) {
            return true;
        }
        return false;
    }

    if (peer.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR ip = peer.toIPv6Address();
        return (ip[0] & 0xfe) == 0xfc;
    }

    return false;
}

QString readable(const QHostAddress &address)
{
    return unmapped(address).toString();
}

}
