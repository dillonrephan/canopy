#pragma once

#include "ProjectedNode.h"

class ProjectedSurfaceGeometry
{
public:
    static void rebuild(ProjectedNode &node, qreal hitPadding);

    [[nodiscard]] static ProjectedNode interpolate(
        const ProjectedNode &from,
        const ProjectedNode &to,
        qreal progress,
        qreal hitPadding);
};
