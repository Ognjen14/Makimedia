#include <QtTest>

#include <QByteArray>
#include <QFile>
#include <QNetworkAccessManager>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrl>

#include "Metadata/PosterCache.h"

namespace {

const QString kKey = QStringLiteral("poster:/abc.jpg@342");
const QString kOtherKey = QStringLiteral("poster:/xyz.jpg@342");
const QString kRemote = QStringLiteral("https://image.tmdb.org/t/p/w342/abc.jpg");

QByteArray aJpeg()
{
    return QByteArray::fromHex("ffd8ffe000104a464946000101") + QByteArray(64, 'j');
}

}

class TestPosterCache : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void recognisesWhatTmdbSends_data();
    void recognisesWhatTmdbSends();

    void nothingIsCachedAtFirst();
    void aStoredPosterComesOffTheDisk();
    void aPageThatIsNotAnImageIsNeverCached();
    void anEmptyFileIsNotAPoster();
    void aFreshCacheLooksAtTheDiskAgain();
    void withoutAKeyTheRemoteImageIsStillShown();
    void withoutARemoteUrlThereIsNothingToShow();
    void clearingForgetsEverything();
    void fetchOnlyStartsWhatIsMissing();
    void aCacheWithNoDirectoryFetchesNothing();

private:
    QString pathOf(const QString &key) const;
    void reopen();

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<QNetworkAccessManager> m_network;
    QScopedPointer<PosterCache> m_cache;
};

void TestPosterCache::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_network.reset(new QNetworkAccessManager);
    reopen();
}

void TestPosterCache::cleanup()
{
    m_cache.reset();
    m_network.reset();
    m_dir.reset();
}

QString TestPosterCache::pathOf(const QString &key) const
{
    return QUrl(m_cache->localUrl(key)).toLocalFile();
}

void TestPosterCache::reopen()
{
    m_cache.reset(new PosterCache(*m_network,
                                  m_dir->path() + QStringLiteral("/posters")));
}

void TestPosterCache::recognisesWhatTmdbSends_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<bool>("image");

    QTest::newRow("a jpeg") << aJpeg() << true;
    QTest::newRow("a png")
        << QByteArray::fromHex("89504e470d0a1a0a0000000d49484452") << true;
    QTest::newRow("a webp")
        << QByteArrayLiteral("RIFF") + QByteArray::fromHex("24000000")
               + QByteArrayLiteral("WEBPVP8 ")
        << true;
    QTest::newRow("a gif") << QByteArrayLiteral("GIF89a\x01\x00\x01\x00") << true;

    QTest::newRow("an html error page")
        << QByteArrayLiteral("<html><body>404 Not Found</body></html>") << false;
    QTest::newRow("a json error")
        << QByteArrayLiteral("{\"status_code\":34}") << false;
    QTest::newRow("nothing at all") << QByteArray() << false;
    QTest::newRow("a riff that is not a webp")
        << QByteArrayLiteral("RIFF") + QByteArray::fromHex("24000000")
               + QByteArrayLiteral("WAVEfmt ")
        << false;
    QTest::newRow("the start of a jpeg marker and nothing else")
        << QByteArray::fromHex("ffd8") << false;
}

void TestPosterCache::recognisesWhatTmdbSends()
{
    QFETCH(QByteArray, bytes);
    QFETCH(bool, image);

    QCOMPARE(PosterCache::looksLikeAnImage(bytes), image);
}

void TestPosterCache::nothingIsCachedAtFirst()
{
    QVERIFY(!m_cache->isCached(kKey));
    QVERIFY(m_cache->localUrl(kKey).isEmpty());
}

void TestPosterCache::aStoredPosterComesOffTheDisk()
{
    QSignalSpy saved(m_cache.data(), &PosterCache::posterSaved);

    QVERIFY(!m_cache->isCached(kKey));
    QVERIFY(m_cache->store(kKey, aJpeg()));

    QCOMPARE(saved.count(), 1);
    QVERIFY(m_cache->isCached(kKey));

    const QString local = m_cache->localUrl(kKey);
    QVERIFY(!local.isEmpty());
    QCOMPARE(saved.first().at(1).toString(), local);

    QFile file(pathOf(kKey));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), aJpeg());

    QCOMPARE(m_cache->urlFor(kRemote, kKey), local);
}

void TestPosterCache::aPageThatIsNotAnImageIsNeverCached()
{
    QSignalSpy saved(m_cache.data(), &PosterCache::posterSaved);

    QVERIFY(!m_cache->store(
        kKey, QByteArrayLiteral("<html><body>503 Service Unavailable</body></html>")));
    QVERIFY(!m_cache->store(kKey, QByteArray()));

    QCOMPARE(saved.count(), 0);
    QVERIFY(!m_cache->isCached(kKey));
    QVERIFY(m_cache->localUrl(kKey).isEmpty());
}

void TestPosterCache::anEmptyFileIsNotAPoster()
{
    QVERIFY(m_cache->store(kKey, aJpeg()));
    const QString path = pathOf(kKey);
    QVERIFY(QFile::exists(path));

    QVERIFY(QFile::resize(path, 0));
    reopen();

    QVERIFY(!m_cache->isCached(kKey));
    QVERIFY(m_cache->localUrl(kKey).isEmpty());
    QVERIFY(!QFile::exists(path));
}

void TestPosterCache::aFreshCacheLooksAtTheDiskAgain()
{
    QVERIFY(m_cache->store(kKey, aJpeg()));
    const QString path = pathOf(kKey);
    QVERIFY(QFile::remove(path));

    reopen();
    QVERIFY(!m_cache->isCached(kKey));

    QVERIFY(m_cache->store(kKey, aJpeg()));
    QVERIFY(m_cache->isCached(kKey));
}

void TestPosterCache::withoutAKeyTheRemoteImageIsStillShown()
{
    QCOMPARE(m_cache->urlFor(kRemote, QString()), kRemote);
    QVERIFY(!m_cache->store(QString(), aJpeg()));
}

void TestPosterCache::withoutARemoteUrlThereIsNothingToShow()
{
    QVERIFY(m_cache->urlFor(QString(), kKey).isEmpty());

    QVERIFY(m_cache->store(kKey, aJpeg()));
    QVERIFY(m_cache->urlFor(QString(), kKey).isEmpty());
}

void TestPosterCache::clearingForgetsEverything()
{
    QVERIFY(m_cache->store(kKey, aJpeg()));
    QVERIFY(m_cache->store(kOtherKey, aJpeg()));
    QVERIFY(m_cache->isCached(kKey));

    m_cache->clear();

    QVERIFY(!m_cache->isCached(kKey));
    QVERIFY(!m_cache->isCached(kOtherKey));
}

void TestPosterCache::fetchOnlyStartsWhatIsMissing()
{
    QVERIFY(m_cache->isUsable());
    QVERIFY(!m_cache->fetch(QString(), kKey));
    QVERIFY(!m_cache->fetch(kRemote, QString()));

    const QString unreachable = QStringLiteral("http://127.0.0.1:9/xyz.jpg");
    QVERIFY(m_cache->fetch(unreachable, kOtherKey));
    QVERIFY(!m_cache->fetch(unreachable, kOtherKey));

    QVERIFY(m_cache->store(kKey, aJpeg()));
    QVERIFY(!m_cache->fetch(kRemote, kKey));
}

void TestPosterCache::aCacheWithNoDirectoryFetchesNothing()
{
    PosterCache unusable(*m_network, QString());

    QVERIFY(!unusable.isUsable());
    QVERIFY(!unusable.fetch(kRemote, kKey));
    QCOMPARE(unusable.urlFor(kRemote, kKey), kRemote);
}

QTEST_GUILESS_MAIN(TestPosterCache)

#include "tst_postercache.moc"
