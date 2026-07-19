#include "DirectoryScanController.h"
#include "DirectoryScanner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class DirectoryScannerTest final : public QObject
{
    Q_OBJECT

private slots:
    void scansClassifiesAndSortsEntries();
    void boundsMaterializedEntriesWhileCountingAll();
    void emitsIncrementalBoundedGrowth();
    void scansDeterministicPagesWithoutGrowingMemory();
    void reportsInvalidPathsLiterally();
    void supportsCancellation();
    void controllerDeliversOnlyTheLatestRequest();
};

namespace {
void createFile(const QString &path, const QByteArray &contents = {})
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}
}

void DirectoryScannerTest::scansClassifiesAndSortsEntries()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkdir(QStringLiteral("Zebra")));
    QVERIFY(root.mkdir(QStringLiteral("apple")));
    createFile(root.filePath(QStringLiteral("photo.png")), "not-real-image-data");
    createFile(root.filePath(QStringLiteral("notes.txt")), "hello");
    createFile(root.filePath(QStringLiteral("main.cpp")), "int main() {}");

    const DirectoryScanResult result = DirectoryScanner::scan(root.path());

    QCOMPARE(result.state, DirectoryLoadState::Ready);
    QCOMPARE(result.totalEntryCount, quint64(5));
    QCOMPARE(result.entries.size(), 5);
    QCOMPARE(result.entries.at(0).name, QStringLiteral("apple"));
    QCOMPARE(result.entries.at(0).kind, NodeKind::Directory);
    QCOMPARE(result.entries.at(1).name, QStringLiteral("Zebra"));
    QCOMPARE(result.entries.at(1).kind, NodeKind::Directory);

    const auto code = std::ranges::find_if(
        result.entries, [](const ScannedEntry &entry) {
            return entry.name == QStringLiteral("main.cpp");
        });
    QVERIFY(code != result.entries.cend());
    QCOMPARE(code->kind, NodeKind::Code);

    const auto image = std::ranges::find_if(
        result.entries, [](const ScannedEntry &entry) {
            return entry.name == QStringLiteral("photo.png");
        });
    QVERIFY(image != result.entries.cend());
    QCOMPARE(image->kind, NodeKind::Image);
}

void DirectoryScannerTest::boundsMaterializedEntriesWhileCountingAll()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 39; index >= 0; --index) {
        createFile(root.filePath(
            QStringLiteral("entry-%1.txt").arg(index, 2, 10, QLatin1Char('0'))));
    }

    const DirectoryScanResult result = DirectoryScanner::scan(root.path(), 10);

    QCOMPARE(result.state, DirectoryLoadState::Partial);
    QCOMPARE(result.totalEntryCount, quint64(40));
    QCOMPARE(result.entries.size(), 10);
    for (qsizetype index = 1; index < result.entries.size(); ++index) {
        QVERIFY(result.entries.at(index - 1).orderingKey
                < result.entries.at(index).orderingKey);
    }
    QVERIFY(!result.status.isEmpty());
}

void DirectoryScannerTest::emitsIncrementalBoundedGrowth()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 0; index < 23; ++index)
        createFile(root.filePath(QStringLiteral("growth-%1").arg(index)));

    QVector<DirectoryScanUpdate> updates;
    const DirectoryScanResult result = DirectoryScanner::scan(
        root.path(), 10, {}, [&updates](const DirectoryScanUpdate &update) {
            updates.append(update);
        });

    QCOMPARE(result.totalEntryCount, quint64(23));
    QCOMPARE(result.entries.size(), 10);
    QVERIFY(updates.size() >= 3);
    qsizetype deliveredEntries = 0;
    quint64 previousCount = 0;
    for (const DirectoryScanUpdate &update : updates) {
        QVERIFY(update.entries.size() <= 4);
        QVERIFY(update.discoveredEntryCount >= previousCount);
        deliveredEntries += update.entries.size();
        previousCount = update.discoveredEntryCount;
    }
    QCOMPARE(deliveredEntries, 10);
}

void DirectoryScannerTest::scansDeterministicPagesWithoutGrowingMemory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 39; index >= 0; --index) {
        createFile(root.filePath(
            QStringLiteral("entry-%1.txt").arg(index, 2, 10, QLatin1Char('0'))));
    }

    const DirectoryPageResult first =
        DirectoryScanner::scanPage(root.path(), {}, 10);
    QCOMPARE(first.state, DirectoryLoadState::Ready);
    QCOMPARE(first.totalEntryCount, quint64(40));
    QCOMPARE(first.entries.size(), 10);
    QCOMPARE(first.entries.first().name, QStringLiteral("entry-00.txt"));
    QCOMPARE(first.entries.last().name, QStringLiteral("entry-09.txt"));
    QVERIFY(first.hasNextPage);

    const DirectoryPageResult second =
        DirectoryScanner::scanPage(root.path(), first.nextCursor, 10);
    QCOMPARE(second.totalEntryCount, quint64(40));
    QCOMPARE(second.entries.size(), 10);
    QCOMPARE(second.entries.first().name, QStringLiteral("entry-10.txt"));
    QCOMPARE(second.entries.last().name, QStringLiteral("entry-19.txt"));
    QVERIFY(second.hasNextPage);
}

void DirectoryScannerTest::reportsInvalidPathsLiterally()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString missing = temporaryDirectory.filePath(QStringLiteral("missing"));

    const DirectoryScanResult result = DirectoryScanner::scan(missing);

    QCOMPARE(result.state, DirectoryLoadState::Error);
    QVERIFY(result.entries.isEmpty());
    QVERIFY(result.status.contains(QStringLiteral("no longer exists")));
}

void DirectoryScannerTest::supportsCancellation()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 0; index < 8; ++index)
        createFile(root.filePath(QStringLiteral("entry-%1").arg(index)));

    int checks = 0;
    const DirectoryScanResult result = DirectoryScanner::scan(
        root.path(), 10, [&checks] { return ++checks > 2; });

    QVERIFY(result.cancelled);
    QVERIFY(result.totalEntryCount < 8);
    QCOMPARE(result.status, QStringLiteral("Scan cancelled."));
}

void DirectoryScannerTest::controllerDeliversOnlyTheLatestRequest()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    createFile(temporaryDirectory.filePath(QStringLiteral("seed.txt")), "seed");

    DirectoryScanController controller;
    QVector<DirectoryScanResult> delivered;
    connect(&controller, &DirectoryScanController::scanFinished,
            this, [&delivered](quint64, const DirectoryScanResult &result) {
                delivered.append(result);
            });

    controller.requestScan(
        temporaryDirectory.filePath(QStringLiteral("already-gone")));
    controller.requestScan(temporaryDirectory.path());

    QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 1, 3000);
    QCOMPARE(delivered.first().path, temporaryDirectory.path());
    QCOMPARE(delivered.first().state, DirectoryLoadState::Ready);
    QCOMPARE(delivered.first().totalEntryCount, quint64(1));
}

QTEST_GUILESS_MAIN(DirectoryScannerTest)

#include "DirectoryScannerTest.moc"
