#pragma once

#include <QString>

namespace ArtworkLookup {

QString cacheFileName(const QString &key);

QString fileFor(const QString &cacheDir, const QString &key);

}
