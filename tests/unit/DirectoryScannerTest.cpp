#include "DirectoryScanController.h"
#include "DirectoryScanner.h"
#include "DirectoryContentsModel.h"
#include "FilesystemTreeModel.h"

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
    void hiddenEntriesFollowVisibilityPolicy();
    void manuallyResolvedHiddenDirectoryRemainsReachable();
    void reportsInvalidPathsLiterally();
    void supportsCancellation();
    void controllerDeliversOnlyTheLatestRequest();
    void contentsModelPagesWithoutRetainingEarlierPages();
    void filesystemModelReportsExactReconciliationDeltas();
    void filesystemModelObservesExternalDirectoryChanges();
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

void DirectoryScannerTest::hiddenEntriesFollowVisibilityPolicy()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    createFile(
        temporaryDirectory.filePath(QStringLiteral("visible.txt")), "visible");
    createFile(
        temporaryDirectory.filePath(QStringLiteral(".secret.txt")), "hidden");

    const DirectoryScanResult defaultScan =
        DirectoryScanner::scan(temporaryDirectory.path());
    QCOMPARE(defaultScan.totalEntryCount, quint64(1));

    const DirectoryScanResult hiddenScan = DirectoryScanner::scan(
        temporaryDirectory.path(),
        DirectoryScanner::DefaultMaterializedEntryLimit,
        {},
        {},
        true);
    QCOMPARE(hiddenScan.totalEntryCount, quint64(2));

    FilesystemTreeModel model(temporaryDirectory.path());
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(1), 3000);
    model.setContentsDirectory(model.rootId());
    model.setIncludeHidden(true);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(2), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.contentsModel()->totalEntryCount(), qulonglong(2), 3000);
}

void DirectoryScannerTest::manuallyResolvedHiddenDirectoryRemainsReachable()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral(".garden/nested")));

    FilesystemTreeModel model(root.path());
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->directoryState,
        DirectoryLoadState::Ready,
        3000);
    QCOMPARE(model.node(model.rootId())->totalChildCount, quint64(0));

    QString error;
    const QString hiddenPath = root.filePath(QStringLiteral(".garden/nested"));
    const NodeId hiddenId = model.ensureDirectory(hiddenPath, &error);
    QVERIFY2(hiddenId != InvalidNodeId, qPrintable(error));
    QCOMPARE(model.node(hiddenId)->path, hiddenPath);
    QVERIFY(model.node(hiddenId)->parentId != InvalidNodeId);

    model.setActiveDirectory(hiddenId);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(hiddenId)->directoryState,
        DirectoryLoadState::Ready,
        3000);
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

void DirectoryScannerTest::contentsModelPagesWithoutRetainingEarlierPages()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 0; index < 150; ++index) {
        createFile(root.filePath(
            QStringLiteral("entry-%1.txt").arg(index, 3, 10, QLatin1Char('0'))));
    }

    DirectoryContentsModel model;
    model.setDirectory(root.path());
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), int(DirectoryContentsModel::PageSize));
    QCOMPARE(model.pageNumber(), 1);
    QCOMPARE(model.totalEntryCount(), qulonglong(150));
    QVERIFY(model.canNext());
    QVERIFY(!model.canPrevious());
    QCOMPARE(
        model.data(model.index(0), DirectoryContentsModel::NameRole).toString(),
        QStringLiteral("entry-000.txt"));

    model.nextPage();
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), int(DirectoryContentsModel::PageSize));
    QCOMPARE(model.pageNumber(), 2);
    QVERIFY(model.canPrevious());
    QCOMPARE(
        model.data(model.index(0), DirectoryContentsModel::NameRole).toString(),
        QStringLiteral("entry-064.txt"));

    model.previousPage();
    QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 3000);
    QCOMPARE(model.rowCount(), int(DirectoryContentsModel::PageSize));
    QCOMPARE(model.pageNumber(), 1);
    QCOMPARE(
        model.data(model.index(0), DirectoryContentsModel::NameRole).toString(),
        QStringLiteral("entry-000.txt"));
}

void DirectoryScannerTest::filesystemModelReportsExactReconciliationDeltas()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    for (int index = 0; index < 10; ++index) {
        createFile(root.filePath(
            QStringLiteral("entry-%1.txt").arg(
                index, 2, 10, QLatin1Char('0'))));
    }

    FilesystemTreeModel model(root.path());
    QVector<TreeChangeSet> changes;
    connect(&model, &FilesystemTreeModel::treeChanged,
            this, [&changes](const TreeChangeSet &change) {
                changes.append(change);
            });
    const auto hasReconciliation = [&changes] {
        return std::ranges::any_of(
            changes, [](const TreeChangeSet &change) {
                return change.reason == TreeChangeReason::Reconciled;
            });
    };
    const auto latestReconciliation = [&changes] {
        return std::ranges::find_if(
            changes.crbegin(), changes.crend(),
            [](const TreeChangeSet &change) {
                return change.reason == TreeChangeReason::Reconciled;
            });
    };

    QTRY_VERIFY_WITH_TIMEOUT(hasReconciliation(), 3000);
    QCOMPARE(model.node(model.rootId())->children.size(), qsizetype(10));
    const auto initialReconciliation = latestReconciliation();
    QVERIFY(initialReconciliation != changes.crend());
    QVERIFY(initialReconciliation->added.isEmpty());
    QVERIFY(initialReconciliation->removed.isEmpty());
    QVERIFY(std::ranges::any_of(
        changes, [](const TreeChangeSet &change) {
            return change.reason == TreeChangeReason::Progress
                && !change.added.isEmpty();
        }));

    changes.clear();
    model.setIncludeHidden(true);
    QTRY_VERIFY_WITH_TIMEOUT(hasReconciliation(), 3000);
    const auto unchangedReconciliation = latestReconciliation();
    QVERIFY(unchangedReconciliation != changes.crend());
    QVERIFY(unchangedReconciliation->added.isEmpty());
    QVERIFY(unchangedReconciliation->removed.isEmpty());

    changes.clear();
    const QString addedPath =
        root.filePath(QStringLiteral("entry-10.txt"));
    createFile(addedPath, "new growth");
    QTRY_VERIFY_WITH_TIMEOUT(hasReconciliation(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(11), 5000);
    const auto addedNode = std::ranges::find_if(
        model.nodes(), [&addedPath](const FileNode &node) {
            return node.path == addedPath;
        });
    QVERIFY(addedNode != model.nodes().cend());
    QSet<NodeId> reportedAdded;
    QSet<NodeId> reportedRemoved;
    for (const TreeChangeSet &change : std::as_const(changes)) {
        for (const NodeId id : change.added)
            reportedAdded.insert(id);
        for (const NodeId id : change.removed)
            reportedRemoved.insert(id);
    }
    QCOMPARE(reportedAdded, QSet<NodeId>({addedNode->id}));
    QVERIFY(reportedRemoved.isEmpty());
    const auto additionReconciliation = latestReconciliation();
    QVERIFY(additionReconciliation != changes.crend());
    QVERIFY(additionReconciliation->added.isEmpty());
    QVERIFY(additionReconciliation->removed.isEmpty());

    const NodeId removedId = addedNode->id;
    changes.clear();
    QVERIFY(QFile::remove(addedPath));
    QTRY_VERIFY_WITH_TIMEOUT(hasReconciliation(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(10), 5000);
    reportedAdded.clear();
    reportedRemoved.clear();
    for (const TreeChangeSet &change : std::as_const(changes)) {
        for (const NodeId id : change.added)
            reportedAdded.insert(id);
        for (const NodeId id : change.removed)
            reportedRemoved.insert(id);
    }
    QVERIFY(reportedAdded.isEmpty());
    QCOMPARE(reportedRemoved, QSet<NodeId>({removedId}));
    const auto removalReconciliation = latestReconciliation();
    QVERIFY(removalReconciliation != changes.crend());
    QVERIFY(removalReconciliation->added.isEmpty());
    QCOMPARE(removalReconciliation->removed, QVector<NodeId>({removedId}));
}

void DirectoryScannerTest::filesystemModelObservesExternalDirectoryChanges()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    FilesystemTreeModel model(temporaryDirectory.path());
    QVector<TreeChangeSet> changes;
    connect(&model, &FilesystemTreeModel::treeChanged,
            this, [&changes](const TreeChangeSet &change) {
                changes.append(change);
            });

    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->directoryState,
        DirectoryLoadState::Ready,
        3000);
    QCOMPARE(model.node(model.rootId())->totalChildCount, quint64(0));

    const QString sproutPath =
        temporaryDirectory.filePath(QStringLiteral("sprout.txt"));
    createFile(sproutPath, "new growth");
    QTRY_VERIFY_WITH_TIMEOUT(
        std::ranges::any_of(
            model.nodes(), [&sproutPath](const FileNode &node) {
                return node.path == sproutPath
                    && node.parentId == NodeId(1);
            }),
        5000);
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(1), 5000);
    QVERIFY(std::ranges::any_of(changes, [](const TreeChangeSet &change) {
        return !change.added.isEmpty();
    }));

    const QString renamedPath =
        temporaryDirectory.filePath(QStringLiteral("flower.txt"));
    QVERIFY(QFile::rename(sproutPath, renamedPath));
    QTRY_VERIFY_WITH_TIMEOUT(
        std::ranges::any_of(
            model.node(model.rootId())->children,
            [&model, &renamedPath](NodeId childId) {
                const FileNode *child = model.node(childId);
                return child && child->path == renamedPath;
            }),
        5000);
    QVERIFY(std::ranges::none_of(
        model.node(model.rootId())->children,
        [&model, &sproutPath](NodeId childId) {
            const FileNode *child = model.node(childId);
            return child && child->path == sproutPath;
        }));

    QVERIFY(QFile::remove(renamedPath));
    QTRY_COMPARE_WITH_TIMEOUT(
        model.node(model.rootId())->totalChildCount, quint64(0), 5000);
    QVERIFY(std::ranges::any_of(changes, [](const TreeChangeSet &change) {
        return !change.removed.isEmpty();
    }));
    QVERIFY(std::ranges::none_of(
        model.node(model.rootId())->children,
        [&model, &sproutPath](NodeId childId) {
            const FileNode *child = model.node(childId);
            return child && child->path == sproutPath;
        }));
}

QTEST_GUILESS_MAIN(DirectoryScannerTest)

#include "DirectoryScannerTest.moc"
