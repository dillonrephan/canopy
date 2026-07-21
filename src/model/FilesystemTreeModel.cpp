#include "FilesystemTreeModel.h"

#include "DirectoryPresentationPolicy.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <functional>
#include <utility>

FilesystemTreeModel::FilesystemTreeModel(QObject *parent)
    : FilesystemTreeModel(QDir::homePath(), parent)
{
}

FilesystemTreeModel::FilesystemTreeModel(
    const QString &rootPath, QObject *parent)
    : QObject(parent)
{
    const QString cleanRootPath = QDir::cleanPath(
        QFileInfo(rootPath).absoluteFilePath());
    const QString rootName = QFileInfo(cleanRootPath).fileName();
    m_nodes.append({
        .id = rootId(),
        .parentId = InvalidNodeId,
        .name = rootName.isEmpty() ? QStringLiteral("Home") : rootName,
        .path = cleanRootPath,
        .kind = NodeKind::Directory,
        .detail = QStringLiteral("Read-only filesystem root"),
        .directoryState = DirectoryLoadState::Loading,
        .directoryStatus = QStringLiteral("Reading directory contents…")
    });

    m_watchDebounce.setSingleShot(true);
    m_watchDebounce.setInterval(140);
    connect(&m_watchDebounce, &QTimer::timeout,
            this, &FilesystemTreeModel::processWatchedChanges);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &path) {
                m_pendingWatchedPaths.insert(QDir::cleanPath(path));
                m_watchDebounce.start();
            });
    connect(&m_scanController, &DirectoryScanController::scanUpdated,
            this, [this](quint64 requestId, const DirectoryScanUpdate &update) {
                if (requestId != m_activeScanRequest
                    || m_scanningNodeId == InvalidNodeId) {
                    return;
                }
                applyScanUpdate(m_scanningNodeId, update);
            });
    connect(&m_scanController, &DirectoryScanController::scanFinished,
            this, [this](quint64 requestId, const DirectoryScanResult &result) {
                if (requestId != m_activeScanRequest
                    || m_scanningNodeId == InvalidNodeId) {
                    return;
                }
                const NodeId scannedNodeId = m_scanningNodeId;
                m_scanningNodeId = InvalidNodeId;
                applyScanResult(scannedNodeId, result);
                updateWatchedDirectories();
            });

    setActiveDirectory(rootId());
}

const FileNode *FilesystemTreeModel::node(NodeId id) const
{
    for (const FileNode &entry : m_nodes) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

FileNode *FilesystemTreeModel::mutableNode(NodeId id)
{
    for (FileNode &entry : m_nodes) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

NodeId FilesystemTreeModel::ensureDirectory(
    const QString &path, QString *errorMessage)
{
    const QFileInfo requested(path);
    if (!requested.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("That path does not exist.");
        return InvalidNodeId;
    }
    if (!requested.isDir()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("That path is not a directory.");
        return InvalidNodeId;
    }
    if (!requested.isReadable()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("That directory is not readable.");
        return InvalidNodeId;
    }

    const QString targetPath = QDir::cleanPath(requested.absoluteFilePath());
    if (const NodeId existing = nodeIdForPath(targetPath);
        existing != InvalidNodeId) {
        return existing;
    }

    QStringList missingPaths;
    QString cursor = targetPath;
    NodeId parentId = InvalidNodeId;
    while (true) {
        if (const NodeId existing = nodeIdForPath(cursor);
            existing != InvalidNodeId) {
            parentId = existing;
            break;
        }
        missingPaths.prepend(cursor);
        const QString parentPath = QFileInfo(cursor).dir().absolutePath();
        if (parentPath == cursor)
            break;
        cursor = parentPath;
    }

    for (const QString &missingPath : missingPaths) {
        const QFileInfo info(missingPath);
        const NodeId id = m_nextNodeId++;
        m_nodes.append({
            .id = id,
            .parentId = parentId,
            .name = info.fileName().isEmpty()
                ? missingPath : info.fileName(),
            .path = missingPath,
            .kind = NodeKind::Directory,
            .detail = info.isSymbolicLink()
                ? QStringLiteral("Symbolic link directory")
                : QStringLiteral("Directory"),
            .directoryState = DirectoryLoadState::Unloaded,
            .directoryStatus = QStringLiteral("Open this branch to scan it."),
            .isSymbolicLink = info.isSymbolicLink()
        });
        if (FileNode *parent = mutableNode(parentId);
            parent && !parent->children.contains(id)) {
            parent->children.append(id);
        }
        parentId = id;
    }

    emit treeChanged({
        .reason = TreeChangeReason::Reset,
        .directoryId = parentId
    });
    return parentId;
}

void FilesystemTreeModel::setActiveDirectory(NodeId id)
{
    FileNode *directory = mutableNode(id);
    if (!directory || !directory->isDirectory())
        return;

    m_activeDirectoryId = id;
    updateWatchedDirectories();
    if (directory->directoryState == DirectoryLoadState::Unloaded
        || (id == rootId() && directory->children.isEmpty())) {
        scanDirectory(id);
    }
}

void FilesystemTreeModel::setContentsDirectory(NodeId id)
{
    const FileNode *directory = node(id);
    m_contentsDirectoryId =
        directory && directory->isDirectory() ? id : InvalidNodeId;
    m_contentsModel.setDirectory(
        m_contentsDirectoryId == InvalidNodeId
            ? QString() : directory->path);
    updateWatchedDirectories();
}

void FilesystemTreeModel::setIncludeHidden(bool includeHidden)
{
    if (m_includeHidden == includeHidden)
        return;
    m_includeHidden = includeHidden;
    m_contentsModel.setIncludeHidden(includeHidden);
    if (m_activeDirectoryId != InvalidNodeId)
        scanDirectory(m_activeDirectoryId);
}

void FilesystemTreeModel::scanDirectory(NodeId id)
{
    FileNode *directory = mutableNode(id);
    if (!directory || !directory->isDirectory())
        return;

    if (m_scanningNodeId != InvalidNodeId) {
        if (FileNode *stale = mutableNode(m_scanningNodeId);
            stale && stale->directoryState == DirectoryLoadState::Loading) {
            stale->directoryState = stale->children.isEmpty()
                ? DirectoryLoadState::Unloaded
                : stale->children.size()
                    < static_cast<qsizetype>(stale->totalChildCount)
                    ? DirectoryLoadState::Partial
                    : DirectoryLoadState::Ready;
            stale->directoryStatus =
                QStringLiteral("Background scan superseded by navigation.");
        }
        m_scanController.cancel();
    }

    directory->directoryState = DirectoryLoadState::Loading;
    directory->directoryStatus = QStringLiteral("Reading directory contents…");
    m_scanningNodeId = id;
    m_activeScanRequest = m_scanController.requestScan(
        directory->path,
        DirectoryPresentationPolicy::VisualChildLimit,
        m_includeHidden);
    emit treeChanged({
        .reason = TreeChangeReason::Loading,
        .directoryId = id
    });
}

void FilesystemTreeModel::applyScanUpdate(
    NodeId id, const DirectoryScanUpdate &update)
{
    FileNode *directory = mutableNode(id);
    if (!directory)
        return;

    directory->totalChildCount = update.discoveredEntryCount;
    directory->directoryState = DirectoryLoadState::Loading;
    directory->directoryStatus = QStringLiteral("Discovered at least %1 entries…")
                                     .arg(update.discoveredEntryCount);
    const QVector<NodeId> added = appendScannedEntries(
        id, update.entries, false);
    emit treeChanged({
        .reason = TreeChangeReason::Progress,
        .directoryId = id,
        .added = added
    });
}

void FilesystemTreeModel::applyScanResult(
    NodeId id, const DirectoryScanResult &result)
{
    FileNode *directory = mutableNode(id);
    if (!directory)
        return;

    const QVector<NodeId> previousChildren = directory->children;
    QVector<NodeId> pruneRoots;
    for (const NodeId previousId : previousChildren) {
        const FileNode *previous = node(previousId);
        if (!previous || previous->parentId != id)
            continue;
        const auto incoming = std::find_if(
            result.entries.cbegin(), result.entries.cend(),
            [previous](const ScannedEntry &entry) {
                return entry.path == previous->path;
            });
        if (incoming == result.entries.cend()) {
            pruneRoots.append(previousId);
            continue;
        }
        if (previous->kind != incoming->kind)
            pruneRoots.append(previousId);
    }
    const QVector<RemovedTreeNode> removed = removedClosure(
        pruneRoots, id);
    pruneRemovedNodes(removed, id);

    directory = mutableNode(id);
    if (!directory)
        return;
    directory->children.clear();
    directory->totalChildCount = result.totalEntryCount;
    directory->directoryState = result.state;
    directory->directoryStatus = result.status;
    static_cast<void>(appendScannedEntries(id, result.entries, true));
    directory = mutableNode(id);
    if (!directory)
        return;
    reconcileDirectoryOwners(id);
    const QSet<NodeId> previousIds(
        previousChildren.cbegin(), previousChildren.cend());
    QVector<NodeId> added;
    for (const NodeId currentId : std::as_const(directory->children)) {
        if (!previousIds.contains(currentId))
            added.append(currentId);
    }
    emit treeChanged({
        .reason = TreeChangeReason::Reconciled,
        .directoryId = id,
        .added = added,
        .removed = removed
    });
}

QVector<NodeId> FilesystemTreeModel::appendScannedEntries(
    NodeId id,
    const QVector<ScannedEntry> &entries,
    bool finalizeKinds)
{
    QVector<NodeId> added;
    FileNode *directory = mutableNode(id);
    if (!directory)
        return added;

    for (const ScannedEntry &entry : entries) {
        const auto existing = std::find_if(
            m_nodes.begin(), m_nodes.end(), [&entry](const FileNode &candidate) {
                return candidate.path == entry.path;
            });
        NodeId childId = InvalidNodeId;
        if (existing != m_nodes.end()) {
            childId = existing->id;
            existing->parentId = id;
            existing->name = entry.name;
            if (finalizeKinds && existing->kind != entry.kind) {
                existing->kind = entry.kind;
                existing->children.clear();
                existing->totalChildCount = 0;
                existing->directoryState =
                    entry.kind == NodeKind::Directory
                    ? DirectoryLoadState::Unloaded
                    : DirectoryLoadState::Ready;
                existing->directoryStatus =
                    entry.kind == NodeKind::Directory
                    ? QStringLiteral("Open this branch to scan it.")
                    : QString();
            }
            existing->detail = entry.detail;
            existing->isSymbolicLink = entry.isSymbolicLink;
        } else {
            childId = m_nextNodeId++;
            m_nodes.append({
                .id = childId,
                .parentId = id,
                .name = entry.name,
                .path = entry.path,
                .kind = entry.kind,
                .detail = entry.detail,
                .directoryState = entry.kind == NodeKind::Directory
                    ? DirectoryLoadState::Unloaded : DirectoryLoadState::Ready,
                .directoryStatus = entry.kind == NodeKind::Directory
                    ? QStringLiteral("Open this branch to scan it.") : QString(),
                .isSymbolicLink = entry.isSymbolicLink
            });
        }

        directory = mutableNode(id);
        if (!directory->children.contains(childId)) {
            directory->children.append(childId);
            added.append(childId);
        }
    }

    directory = mutableNode(id);
    std::sort(
        directory->children.begin(), directory->children.end(),
        [this](NodeId leftId, NodeId rightId) {
            const FileNode *left = node(leftId);
            const FileNode *right = node(rightId);
            if (!left || !right)
                return leftId < rightId;
            if (left->isDirectory() != right->isDirectory())
                return left->isDirectory();
            const int insensitive = QString::compare(
                left->name, right->name, Qt::CaseInsensitive);
            return insensitive == 0 ? left->name < right->name
                                    : insensitive < 0;
        });
    return added;
}

QVector<RemovedTreeNode> FilesystemTreeModel::removedClosure(
    const QVector<NodeId> &roots,
    NodeId protectedDirectoryId) const
{
    QVector<RemovedTreeNode> removed;
    QSet<NodeId> visited;
    const std::function<void(NodeId)> visit =
        [this, protectedDirectoryId, &removed, &visited, &visit](NodeId id) {
            Q_ASSERT(id != rootId());
            Q_ASSERT(id != protectedDirectoryId);
            if (id == rootId() || id == protectedDirectoryId)
                return;
            if (visited.contains(id))
                return;
            const FileNode *entry = node(id);
            if (!entry)
                return;
            visited.insert(id);
            removed.append({.id = id, .kind = entry->kind});
            for (const NodeId childId : entry->children) {
                const FileNode *child = node(childId);
                if (child && child->parentId == id)
                    visit(childId);
            }
        };
    for (const NodeId root : roots)
        visit(root);
    return removed;
}

void FilesystemTreeModel::pruneRemovedNodes(
    const QVector<RemovedTreeNode> &removed,
    NodeId fallbackDirectoryId)
{
    if (removed.isEmpty())
        return;

    QSet<NodeId> removedIds;
    removedIds.reserve(removed.size());
    for (const RemovedTreeNode &entry : removed)
        removedIds.insert(entry.id);

    for (FileNode &entry : m_nodes) {
        entry.children.removeIf([&removedIds](NodeId childId) {
            return removedIds.contains(childId);
        });
    }

    if (removedIds.contains(m_scanningNodeId)) {
        m_scanController.cancel();
        m_scanningNodeId = InvalidNodeId;
        m_activeScanRequest = 0;
    }
    if (removedIds.contains(m_contentsDirectoryId)) {
        m_contentsDirectoryId = InvalidNodeId;
        m_contentsModel.setDirectory(QString());
    }

    m_nodes.removeIf([&removedIds](const FileNode &entry) {
        return removedIds.contains(entry.id);
    });
    reconcileDirectoryOwners(fallbackDirectoryId);
}

void FilesystemTreeModel::reconcileDirectoryOwners(
    NodeId fallbackDirectoryId)
{
    const FileNode *fallback = node(fallbackDirectoryId);
    const NodeId safeFallback = fallback && fallback->isDirectory()
        ? fallbackDirectoryId : rootId();

    const FileNode *active = node(m_activeDirectoryId);
    if (!active || !active->isDirectory())
        m_activeDirectoryId = safeFallback;

    const FileNode *contents = node(m_contentsDirectoryId);
    if (!contents || !contents->isDirectory()) {
        if (m_contentsDirectoryId != InvalidNodeId)
            m_contentsModel.setDirectory(QString());
        m_contentsDirectoryId = InvalidNodeId;
    }

    const FileNode *scanning = node(m_scanningNodeId);
    if (m_scanningNodeId != InvalidNodeId
        && (!scanning || !scanning->isDirectory())) {
        m_scanController.cancel();
        m_scanningNodeId = InvalidNodeId;
        m_activeScanRequest = 0;
    }
}

void FilesystemTreeModel::updateWatchedDirectories()
{
    QSet<QString> desiredPaths;
    if (const FileNode *active = node(m_activeDirectoryId))
        desiredPaths.insert(active->path);
    if (const FileNode *contents = node(m_contentsDirectoryId))
        desiredPaths.insert(contents->path);

    const QStringList watchedDirectories = m_watcher.directories();
    const QSet<QString> currentPaths(
        watchedDirectories.cbegin(), watchedDirectories.cend());
    const QStringList removals = (currentPaths - desiredPaths).values();
    if (!removals.isEmpty())
        m_watcher.removePaths(removals);

    for (const QString &path : desiredPaths - currentPaths) {
        if (QFileInfo(path).isDir())
            m_watcher.addPath(path);
    }
}

void FilesystemTreeModel::processWatchedChanges()
{
    const QSet<QString> changedPaths = std::exchange(m_pendingWatchedPaths, {});
    for (const QString &path : changedPaths) {
        emit liveDirectoryChanged(path);
        if (const FileNode *active = node(m_activeDirectoryId);
            active && active->path == path) {
            scanDirectory(active->id);
        }
        if (const FileNode *contents = node(m_contentsDirectoryId);
            contents && contents->path == path) {
            m_contentsModel.reload();
        }
    }
}

NodeId FilesystemTreeModel::nodeIdForPath(const QString &path) const
{
    for (const FileNode &entry : m_nodes) {
        if (entry.path == path)
            return entry.id;
    }
    return InvalidNodeId;
}
