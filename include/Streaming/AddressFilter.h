#pragma once

#include <QHostAddress>
#include <QString>

namespace AddressFilter {

bool isPrivatePeer(const QHostAddress &address);

QString readable(const QHostAddress &address);

}
