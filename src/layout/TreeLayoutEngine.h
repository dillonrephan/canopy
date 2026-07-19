#pragma once

#include "PlanarProjection.h"
#include "ProjectedNode.h"
#include "ViewState.h"
#include "WorldLayoutEngine.h"

class TreeLayoutEngine
{
public:
    [[nodiscard]] QVector<ProjectedNode> layout(
        const QVector<FileNode> &nodes,
        NodeId focusedId,
        const ViewState &view = {}) const;

private:
    WorldLayoutEngine m_worldLayoutEngine;
    PlanarProjection m_projection;
};
