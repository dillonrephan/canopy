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
    void filesystemModelPrunesLoadedClosureBeforeEmission();
    void filesystemModelRecreatedPathGetsFreshIds();
    void filesystemModelFinalKindTransitionPrunesDescendants();
};

namespace {
void createFile(const QString &path, const QByteArray &contents = {})
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

const FileNode *nodeForPath(
    const FilesystemTreeModel &model, const QString &path)
{
    const auto found = std::ranges::find_if(
        model.nodes(), [&path](const FileNode &entry) {
            return entry.path == path;
        });
    return found == model.nodes().cend() ? nullptr : &*found;
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
        for (const RemovedTreeNode &entry : change.removed)
            reportedRemoved.insert(entry.id);
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
        for (const RemovedTreeNode &entry : change.removed)
            reportedRemoved.insert(entry.id);
    }
    QVERIFY(reportedAdded.isEmpty());
    QCOMPARE(reportedRemoved, QSet<NodeId>({removedId}));
    const auto removalReconciliation = latestReconciliation();
    QVERIFY(removalReconciliation != changes.crend());
    QVERIFY(removalReconciliation->added.isEmpty());
    QCOMPARE(removalReconciliation->removed.size(), qsizetype(1));
    QCOMPARE(removalReconciliation->removed.first().id, removedId);
    QCOMPARE(
        removalReconciliation->removed.first().kind,
        NodeKind::Document);
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

void DirectoryScannerTest::filesystemModelPrunesLoadedClosureBeforeEmission()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral("branch/nested")));
    const QString branchPath = root.filePath(QStringLiteral("branch"));
    const QString nestedPath = root.filePath(QStringLiteral("branch/nested"));
    const QString leafPath = root.filePath(
        QStringLiteral("branch/nested/leaf.txt"));
    createFile(leafPath, "leaf");

    FilesystemTreeModel model(root.path());
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, branchPath), 3000);
    const NodeId branchId = nodeForPath(model, branchPath)->id;
    model.setActiveDirectory(branchId);
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, nestedPath), 3000);
    const NodeId nestedId = nodeForPath(model, nestedPath)->id;
    model.setActiveDirectory(nestedId);
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, leafPath), 3000);
    const NodeId leafId = nodeForPath(model, leafPath)->id;

    model.setActiveDirectory(model.rootId());
    model.setContentsDirectory(branchId);
    QTRY_VERIFY_WITH_TIMEOUT(!model.contentsModel()->loading(), 3000);
    QCOMPARE(model.activeDirectoryId(), model.rootId());

    QVector<RemovedTreeNode> removed;
    bool prunedBeforeEmission = false;
    connect(&model, &FilesystemTreeModel::treeChanged,
            this, [&model, &removed, &prunedBeforeEmission](
                      const TreeChangeSet &change) {
                if (change.reason != TreeChangeReason::Reconciled
                    || change.removed.isEmpty()) {
                    return;
                }
                removed = change.removed;
                prunedBeforeEmission = std::ranges::all_of(
                    removed, [&model](const RemovedTreeNode &entry) {
                        return model.node(entry.id) == nullptr;
                    });
            });

    QVERIFY(QDir(branchPath).removeRecursively());
    QTRY_VERIFY_WITH_TIMEOUT(model.node(branchId) == nullptr, 5000);
    QVERIFY(prunedBeforeEmission);
    QCOMPARE(removed.size(), qsizetype(3));
    QCOMPARE(removed.at(0).id, branchId);
    QCOMPARE(removed.at(0).kind, NodeKind::Directory);
    QCOMPARE(removed.at(1).id, nestedId);
    QCOMPARE(removed.at(1).kind, NodeKind::Directory);
    QCOMPARE(removed.at(2).id, leafId);
    QCOMPARE(removed.at(2).kind, NodeKind::Document);
    QCOMPARE(model.nodes().size(), qsizetype(1));
    QVERIFY(model.node(model.rootId())->children.isEmpty());
    QCOMPARE(model.activeDirectoryId(), model.rootId());
    QTRY_VERIFY_WITH_TIMEOUT(!model.contentsModel()->loading(), 3000);
    QCOMPARE(model.contentsModel()->rowCount(), 0);
    QCOMPARE(model.contentsModel()->totalEntryCount(), qulonglong(0));
}

void DirectoryScannerTest::filesystemModelRecreatedPathGetsFreshIds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    const QString path = root.filePath(QStringLiteral("repeat.txt"));
    createFile(path, "first");

    FilesystemTreeModel model(root.path());
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, path), 3000);
    NodeId previousId = nodeForPath(model, path)->id;

    for (int iteration = 0; iteration < 5; ++iteration) {
        QVERIFY(QFile::remove(path));
        QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, path) == nullptr, 5000);
        QCOMPARE(model.nodes().size(), qsizetype(1));

        createFile(path, QByteArray::number(iteration));
        QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, path), 5000);
        const NodeId recreatedId = nodeForPath(model, path)->id;
        QVERIFY(recreatedId > previousId);
        previousId = recreatedId;
        QCOMPARE(model.nodes().size(), qsizetype(2));
    }
}

void DirectoryScannerTest::filesystemModelFinalKindTransitionPrunesDescendants()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkpath(QStringLiteral("changing/nested")));
    const QString changingPath = root.filePath(QStringLiteral("changing"));
    const QString nestedPath = root.filePath(QStringLiteral("changing/nested"));
    const QString leafPath = root.filePath(
        QStringLiteral("changing/nested/leaf.txt"));
    createFile(leafPath, "leaf");

    FilesystemTreeModel model(root.path());
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, changingPath), 3000);
    const NodeId changingId = nodeForPath(model, changingPath)->id;
    model.setActiveDirectory(changingId);
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, nestedPath), 3000);
    const NodeId nestedId = nodeForPath(model, nestedPath)->id;
    model.setActiveDirectory(nestedId);
    QTRY_VERIFY_WITH_TIMEOUT(nodeForPath(model, leafPath), 3000);
    const NodeId leafId = nodeForPath(model, leafPath)->id;
    model.setActiveDirectory(model.rootId());

    bool progressKeptDirectoryKind = false;
    QVector<RemovedTreeNode> removed;
    QVector<NodeId> added;
    connect(&model, &FilesystemTreeModel::treeChanged,
            this, [&model, &changingPath, &progressKeptDirectoryKind,
                   &removed, &added](
                      const TreeChangeSet &change) {
                const FileNode *changing = nodeForPath(model, changingPath);
                if (change.reason == TreeChangeReason::Progress && changing)
                    progressKeptDirectoryKind |= changing->isDirectory();
                if (change.reason == TreeChangeReason::Reconciled) {
                    removed = change.removed;
                    added = change.added;
                }
            });

    QVERIFY(QDir(changingPath).removeRecursively());
    createFile(changingPath, "now a file");
    QTRY_VERIFY_WITH_TIMEOUT(
        nodeForPath(model, changingPath)
            && !nodeForPath(model, changingPath)->isDirectory(),
        5000);

    const FileNode *changed = nodeForPath(model, changingPath);
    QVERIFY(changed);
    QVERIFY(changed->id > changingId);
    QCOMPARE(changed->directoryState, DirectoryLoadState::Ready);
    QCOMPARE(changed->totalChildCount, quint64(0));
    QVERIFY(changed->children.isEmpty());
    QVERIFY(changed->directoryStatus.isEmpty());
    QVERIFY(progressKeptDirectoryKind);
    QCOMPARE(removed.size(), qsizetype(3));
    QCOMPARE(removed.at(0).id, changingId);
    QCOMPARE(removed.at(0).kind, NodeKind::Directory);
    QCOMPARE(removed.at(1).id, nestedId);
    QCOMPARE(removed.at(1).kind, NodeKind::Directory);
    QCOMPARE(removed.at(2).id, leafId);
    QCOMPARE(removed.at(2).kind, NodeKind::Document);
    QCOMPARE(added.size(), qsizetype(1));
    QCOMPARE(added.first(), changed->id);
    QVERIFY(!model.node(changingId));
    QVERIFY(!model.node(nestedId));
    QVERIFY(!model.node(leafId));
}

QTEST_GUILESS_MAIN(DirectoryScannerTest)

#include "DirectoryScannerTest.moc"
