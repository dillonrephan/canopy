#pragma once

#include "DirectoryContentsModel.h"
#include "DirectoryScanController.h"
#include "FileNode.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

enum class TreeChangeReason {
    Reset,
    Loading,
    Progress,
    Reconciled
};

struct TreeChangeSet {
    TreeChangeReason reason = TreeChangeReason::Reset;
    NodeId directoryId = InvalidNodeId;
    QVector<NodeId> added;
    QVector<NodeId> removed;
};

Q_DECLARE_METATYPE(TreeChangeSet)

class FilesystemTreeModel final : public QObject
{
    Q_OBJECT

public:
    explicit FilesystemTreeModel(QObject *parent = nullptr);
    explicit FilesystemTreeModel(
        const QString &rootPath, QObject *parent = nullptr);

    [[nodiscard]] NodeId rootId() const noexcept { return 1; }
    [[nodiscard]] const QVector<FileNode> &nodes() const noexcept
    {
        return m_nodes;
    }
    [[nodiscard]] const FileNode *node(NodeId id) const;
    [[nodiscard]] FileNode *mutableNode(NodeId id);
    [[nodiscard]] NodeId ensureDirectory(
        const QString &path, QString *errorMessage = nullptr);
    [[nodiscard]] DirectoryContentsModel *contentsModel() noexcept
    {
        return &m_contentsModel;
    }

    void setActiveDirectory(NodeId id);
    void setContentsDirectory(NodeId id);
    void setIncludeHidden(bool includeHidden);
    [[nodiscard]] bool includeHidden() const noexcept { return m_includeHidden; }

signals:
    void treeChanged(const TreeChangeSet &change);
    void liveDirectoryChanged(const QString &path);

private:
    void scanDirectory(NodeId id);
    void applyScanUpdate(NodeId id, const DirectoryScanUpdate &update);
    void applyScanResult(NodeId id, const DirectoryScanResult &result);
    [[nodiscard]] QVector<NodeId> appendScannedEntries(
        NodeId id, const QVector<ScannedEntry> &entries);
    void updateWatchedDirectories();
    void processWatchedChanges();
    [[nodiscard]] NodeId nodeIdForPath(const QString &path) const;

    QVector<FileNode> m_nodes;
    DirectoryContentsModel m_contentsModel;
    DirectoryScanController m_scanController;
    QFileSystemWatcher m_watcher;
    QTimer m_watchDebounce;
    QSet<QString> m_pendingWatchedPaths;
    NodeId m_activeDirectoryId = InvalidNodeId;
    NodeId m_contentsDirectoryId = InvalidNodeId;
    NodeId m_scanningNodeId = InvalidNodeId;
    NodeId m_nextNodeId = 2;
    quint64 m_activeScanRequest = 0;
    bool m_includeHidden = false;
};
