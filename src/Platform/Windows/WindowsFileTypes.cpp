#include "Platform/Windows/WindowsFileTypes.h"

#include "MmLog.h"
#include "Platform/MediaFileInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

#include <windows.h>
#include <shlobj.h>

namespace {

const QString kProgId = QStringLiteral("Makimedia.Video");

QString classesRoot()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
}

QString executablePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString executableName()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).fileName();
}

void notifyShell()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

}

namespace WindowsFileTypes {

bool isRegistered()
{
    QSettings classes(classesRoot(), QSettings::NativeFormat);
    const QString command =
        classes.value(kProgId + QStringLiteral("/shell/open/command/Default")).toString();
    return command.contains(executablePath(), Qt::CaseInsensitive);
}

bool registerTypes()
{
    QSettings classes(classesRoot(), QSettings::NativeFormat);
    if (!classes.isWritable()) {
        MM_LOG_E() << "cannot write file type registration, registry is read only";
        return false;
    }

    const QString exe = executablePath();
    const QString quotedExe = QChar('"') + exe + QChar('"');

    classes.setValue(kProgId + QStringLiteral("/Default"),
                     QStringLiteral("Makimedia Video"));
    classes.setValue(kProgId + QStringLiteral("/DefaultIcon/Default"),
                     quotedExe + QStringLiteral(",0"));
    classes.setValue(kProgId + QStringLiteral("/shell/open/command/Default"),
                     quotedExe + QStringLiteral(" \"%1\""));

    const QString appKey =
        QStringLiteral("Applications/") + executableName();
    classes.setValue(appKey + QStringLiteral("/shell/open/command/Default"),
                     quotedExe + QStringLiteral(" \"%1\""));
    classes.setValue(appKey + QStringLiteral("/FriendlyAppName"),
                     QStringLiteral("Makimedia"));

    const QStringList suffixes = MediaFormats::videoSuffixes();
    for (const QString &suffix : suffixes) {
        const QString dotted = QStringLiteral(".") + suffix;
        classes.setValue(dotted + QStringLiteral("/OpenWithProgids/") + kProgId,
                         QString());
        classes.setValue(appKey + QStringLiteral("/SupportedTypes/") + dotted,
                         QString());
    }

    classes.sync();
    if (classes.status() != QSettings::NoError) {
        MM_LOG_E() << "file type registration failed, registry status"
                   << classes.status();
        return false;
    }

    notifyShell();
    MM_LOG_I() << "registered" << suffixes.size() << "video types for" << exe;
    return true;
}

bool unregisterTypes()
{
    QSettings classes(classesRoot(), QSettings::NativeFormat);
    if (!classes.isWritable()) {
        return false;
    }

    const QStringList suffixes = MediaFormats::videoSuffixes();
    for (const QString &suffix : suffixes) {
        const QString dotted = QStringLiteral(".") + suffix;
        classes.remove(dotted + QStringLiteral("/OpenWithProgids/") + kProgId);
    }

    classes.remove(QStringLiteral("Applications/") + executableName());
    classes.remove(kProgId);

    classes.sync();
    if (classes.status() != QSettings::NoError) {
        MM_LOG_E() << "removing file type registration failed, registry status"
                   << classes.status();
        return false;
    }

    notifyShell();
    MM_LOG_I() << "removed the video type registration";
    return true;
}

}
