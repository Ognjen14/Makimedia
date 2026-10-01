#pragma once

#include <QByteArray>
#include <QString>

namespace SubtitleEncoding {

QString guess(const QByteArray &sample);
QString decode(const QByteArray &bytes, const QString &encoding);

}
