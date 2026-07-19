#include "TreeLayoutEngine.h"

QVector<ProjectedNode> TreeLayoutEngine::layout(
    const QVector<FileNode> &nodes,
    NodeId focusedId,
    const ViewState &view) const
{
    const WorldLayout world =
        m_worldLayoutEngine.layout(nodes, focusedId);
    return m_projection.project(world, nodes, view);
}
