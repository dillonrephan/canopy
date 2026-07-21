#pragma once

#include "FileNode.h"
#include "ProjectedNode.h"
#include "ViewState.h"
#include "WorldLayout.h"

#include <QVector>

class PerspectiveProjection
{
public:
    [[nodiscard]] QVector<ProjectedNode> project(
        const WorldLayout &world,
        const QVector<FileNode> &nodes,
        const ViewState &view) const;

private:
    [[nodiscard]] static const FileNode *findNode(
        const QVector<FileNode> &nodes, NodeId id);
    [[nodiscard]] static qreal radiusFor(
        const FileNode &node, bool focused);
};
