#include "Library/MediaHandle.h"

#include <QDir>
#include <QUrl>

namespace MediaHandle {

QString fromUrl(const QString &url)
{
    const QUrl parsed(url);
    if (parsed.isLocalFile()) {
        return QDir::fromNativeSeparators(parsed.toLocalFile());
    }
    return QDir::fromNativeSeparators(url);
}

}
