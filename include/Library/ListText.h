#pragma once

#include <QString>

struct LibraryFile;

namespace ListText {

QString clock(double seconds);
QString duration(double seconds);
QString remaining(const LibraryFile &file);
QString location(const QString &parentHandle);
QString prettyTitle(const QString &fileName);

}
