#pragma once

#include "PlanarProjection.h"
#include "PerspectiveProjection.h"
#include "ProjectedNode.h"
#include "ThemeProfile.h"
#include "ViewState.h"
#include "WorldLayoutEngine.h"

#include <optional>

class TreeLayoutEngine
{
public:
    explicit TreeLayoutEngine(
        std::optional<ProjectionKind> projectionOverride = std::nullopt)
        : m_projectionOverride(projectionOverride)
    {
    }

    [[nodiscard]] QVector<ProjectedNode> layout(
        const QVector<FileNode> &nodes,
        NodeId focusedId,
        const ViewState &view = {}) const;

    [[nodiscard]] ProjectionKind projectionKind() const;

private:
    WorldLayoutEngine m_worldLayoutEngine;
    PlanarProjection m_planarProjection;
    PerspectiveProjection m_perspectiveProjection;
    std::optional<ProjectionKind> m_projectionOverride;
};
