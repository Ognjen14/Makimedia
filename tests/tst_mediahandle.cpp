#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include "Library/MediaHandle.h"

class TestMediaHandle : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aPickedFileGetsTheHandleTheScannerStored();
    void aPathWithNativeSeparatorsGetsTheSameHandle();
    void aHandleAlreadyInTheLibraryIsLeftAlone();

    void somethingThatIsNotAFileIsLeftAlone_data();
    void somethingThatIsNotAFileIsLeftAlone();

    void windowsPathsComeOutWithForwardSlashes_data();
    void windowsPathsComeOutWithForwardSlashes();

private:
    QTemporaryDir m_dir;
    QString m_path;
    QString m_stored;
};

void TestMediaHandle::initTestCase()
{
    QVERIFY2(m_dir.isValid(), qPrintable(m_dir.errorString()));

    m_path = m_dir.path() + QStringLiteral("/My Films/Show S01E01.mkv");
    QVERIFY(QDir().mkpath(QFileInfo(m_path).absolutePath()));

    QFile file(m_path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    m_stored = QFileInfo(m_path).absoluteFilePath();
}

void TestMediaHandle::aPickedFileGetsTheHandleTheScannerStored()
{
    const QString url = QUrl::fromLocalFile(m_path).toString();

    QCOMPARE(MediaHandle::fromUrl(url), m_stored);
}

void TestMediaHandle::aPathWithNativeSeparatorsGetsTheSameHandle()
{
    const QString native = QDir::toNativeSeparators(m_path);

    QCOMPARE(MediaHandle::fromUrl(native), m_stored);
}

void TestMediaHandle::aHandleAlreadyInTheLibraryIsLeftAlone()
{
    QCOMPARE(MediaHandle::fromUrl(m_stored), m_stored);
    QCOMPARE(MediaHandle::fromUrl(MediaHandle::fromUrl(m_stored)), m_stored);
}

void TestMediaHandle::somethingThatIsNotAFileIsLeftAlone_data()
{
    QTest::addColumn<QString>("url");

    QTest::newRow("nothing") << QString();
    QTest::newRow("an android document")
        << QStringLiteral("content://com.android.externalstorage.documents/"
                          "document/primary%3AMovies%2Fa.mkv");
    QTest::newRow("a stream") << QStringLiteral("https://example.com/a.mkv");
}

void TestMediaHandle::somethingThatIsNotAFileIsLeftAlone()
{
    QFETCH(QString, url);

    QCOMPARE(MediaHandle::fromUrl(url), url);
}

void TestMediaHandle::windowsPathsComeOutWithForwardSlashes_data()
{
#ifndef Q_OS_WIN
    QSKIP("drive letters and UNC shares only mean something on Windows");
#endif

    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("handle");

    QTest::newRow("a path from Explorer")
        << QStringLiteral("E:\\TV\\Show\\ep01.mkv")
        << QStringLiteral("E:/TV/Show/ep01.mkv");
    QTest::newRow("a file dialog url")
        << QStringLiteral("file:///E:/TV/Show/ep01.mkv")
        << QStringLiteral("E:/TV/Show/ep01.mkv");
    QTest::newRow("a url with spaces encoded")
        << QStringLiteral("file:///E:/My%20Films/a%20b.mkv")
        << QStringLiteral("E:/My Films/a b.mkv");
    QTest::newRow("already forward")
        << QStringLiteral("E:/TV/Show/ep01.mkv")
        << QStringLiteral("E:/TV/Show/ep01.mkv");
    QTest::newRow("a network share")
        << QStringLiteral("\\\\server\\share\\TV\\ep01.mkv")
        << QStringLiteral("//server/share/TV/ep01.mkv");
}

void TestMediaHandle::windowsPathsComeOutWithForwardSlashes()
{
    QFETCH(QString, input);
    QFETCH(QString, handle);

    QCOMPARE(MediaHandle::fromUrl(input), handle);
}

QTEST_APPLESS_MAIN(TestMediaHandle)

#include "tst_mediahandle.moc"
