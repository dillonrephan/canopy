#pragma once

#include "DirectoryContentsModel.h"
#include "DirectoryScanController.h"
#include "FileNode.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>

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
    [[nodiscard]] DirectoryContentsModel *contentsModel() noexcept
    {
        return &m_contentsModel;
    }

    void setActiveDirectory(NodeId id);
    void setContentsDirectory(NodeId id);

signals:
    void treeChanged();
    void liveDirectoryChanged(const QString &path);

private:
    void scanDirectory(NodeId id);
    void applyScanUpdate(NodeId id, const DirectoryScanUpdate &update);
    void applyScanResult(NodeId id, const DirectoryScanResult &result);
    void appendScannedEntries(NodeId id, const QVector<ScannedEntry> &entries);
    void updateWatchedDirectories();
    void processWatchedChanges();

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
};
