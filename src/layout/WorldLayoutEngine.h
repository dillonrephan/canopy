#pragma once

#include "FileNode.h"
#include "WorldLayout.h"

#include <QHash>
#include <QVector>

class WorldLayoutEngine
{
public:
    [[nodiscard]] WorldLayout layout(
        const QVector<FileNode> &nodes, NodeId focusedId) const;

private:
    [[nodiscard]] static const FileNode *findNode(
        const QVector<FileNode> &nodes, NodeId id);
    [[nodiscard]] QVector<int> corridorSlots(
        const QVector<const FileNode *> &children) const;

    mutable QHash<NodeId, int> m_corridorSlots;
};
