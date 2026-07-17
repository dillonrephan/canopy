#pragma once

#include <QString>
#include <QVector>
#include <QtTypes>

using NodeId = quint64;
inline constexpr NodeId InvalidNodeId = 0;

enum class NodeKind {
    Directory,
    Document,
    Image,
    Audio,
    Code,
    Archive
};

struct FileNode {
    NodeId id = InvalidNodeId;
    NodeId parentId = InvalidNodeId;
    QString name;
    QString path;
    NodeKind kind = NodeKind::Directory;
    QString detail;
    QVector<NodeId> children;

    [[nodiscard]] bool isDirectory() const noexcept
    {
        return kind == NodeKind::Directory;
    }
};
