#include "MmLog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>

#include <cstdio>

#ifdef MM_LOG_VERBOSE
Q_LOGGING_CATEGORY(mmLogCat, "mm", QtDebugMsg)
#else
Q_LOGGING_CATEGORY(mmLogCat, "mm", QtInfoMsg)
#endif

namespace {
constexpr qint64 kMaxBytes = 2 * 1024 * 1024;
constexpr QIODevice::OpenMode kOpenMode =
    QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text | QIODevice::Unbuffered;

QtMessageHandler s_previous = nullptr;
QFile s_file;
QMutex s_mutex;
qint64 s_bytes = 0;

void rotateIfNeeded()
{
    if (s_bytes < kMaxBytes) {
        return;
    }

    const QString path = s_file.fileName();
    const QString previous = path + QStringLiteral(".1");

    s_file.close();

    QFile::remove(previous);
    if (!QFile::rename(path, previous)) {
        QFile::remove(path);
    }

    s_file.setFileName(path);
    if (!s_file.open(kOpenMode)) {
        fprintf(stderr, "MM_LOG could not reopen %s after rotating it\n",
                qPrintable(path));
        fflush(stderr);
    }
    s_bytes = 0;
}

void mmMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (s_previous)
        s_previous(type, ctx, msg);

    QMutexLocker locker(&s_mutex);
    if (!s_file.isOpen())
        return;

    const QByteArray line = qFormatLogMessage(type, ctx, msg).toUtf8() + '\n';
    s_file.write(line);
    s_bytes += line.size();

    rotateIfNeeded();
}
}

void MmLog::install(bool alsoWriteToFile, const QString &filePath)
{
#ifdef MM_LOG_ENABLED
    static bool installed = false;
    if (installed)
        return;
    installed = true;

    const QString defaultRoot =
#ifdef Q_OS_ANDROID
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif

    const QString path = filePath.isEmpty()
        ? defaultRoot + QStringLiteral("/logs/makimedia.log")
        : filePath;

    if (qEnvironmentVariableIsEmpty("QT_MESSAGE_PATTERN")) {
        qSetMessagePattern(QStringLiteral(
            "[%{time hh:mm:ss.zzz}]"
            "[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}"
            "%{if-critical}E%{endif}%{if-fatal}F%{endif}]"
            "[%{category}] %{message}   (%{file}:%{line})"));
    }

    if (alsoWriteToFile) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        s_file.setFileName(path);
        const bool ok = s_file.open(kOpenMode);
        if (!ok) {
            qCritical() << "MM_LOG file did not open on install!";
        } else {
            s_bytes = s_file.size();
        }
    }

    s_previous = qInstallMessageHandler(mmMessageHandler);
#else
    Q_UNUSED(alsoWriteToFile)
    Q_UNUSED(filePath)
#endif
}

QString MmLog::logFilePath()
{
    QMutexLocker locker(&s_mutex);
    return s_file.isOpen() ? s_file.fileName() : QString();
}
