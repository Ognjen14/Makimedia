#pragma once

#include <QLoggingCategory>
#include <QDebug>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(mmLogCat)

namespace MmLog {
void install(bool alsoWriteToFile = true, const QString &filePath = QString());
QString logFilePath();
}

#ifdef MM_LOG_ENABLED
#  define MM_LOG_D() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).debug(mmLogCat)
#  define MM_LOG_I() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).info(mmLogCat)
#  define MM_LOG_W() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).warning(mmLogCat)
#  define MM_LOG_E() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).critical(mmLogCat)
#else
#  define MM_LOG_D() QT_NO_QDEBUG_MACRO()
#  define MM_LOG_I() QT_NO_QDEBUG_MACRO()
#  define MM_LOG_W() QT_NO_QDEBUG_MACRO()
#  define MM_LOG_E() QT_NO_QDEBUG_MACRO()
#endif

#define MM_LOG() MM_LOG_D()
