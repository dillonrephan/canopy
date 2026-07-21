#include "TreeLayoutEngine.h"

#include "ThemeRegistry.h"

QVector<ProjectedNode> TreeLayoutEngine::layout(
    const QVector<FileNode> &nodes,
    NodeId focusedId,
    const ViewState &view) const
{
    const WorldLayout world =
        m_worldLayoutEngine.layout(nodes, focusedId);
    if (projectionKind() == ProjectionKind::Perspective)
        return m_perspectiveProjection.project(world, nodes, view);
    return m_planarProjection.project(world, nodes, view);
}

ProjectionKind TreeLayoutEngine::projectionKind() const
{
    return m_projectionOverride.value_or(ThemeRegistry::active().projection);
}
