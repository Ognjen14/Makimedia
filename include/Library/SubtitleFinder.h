#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace SubtitleFinder {

using IsVideo = std::function<bool(const QString &fileName)>;

const QStringList &subtitleFolderNames();

bool isSubtitleFolderName(const QString &name);

QStringList onDisk(const QString &videoPath, const IsVideo &isVideo);

}
