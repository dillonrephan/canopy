#include "PerspectiveProjection.h"

#include "ThemeRegistry.h"

#include <QHash>
#include <QtMath>

namespace {
struct WorldPoint {
    qreal lateral = 0.0;
    qreal depth = 0.0;
};

struct ScreenPoint {
    QPointF position;
    qreal scale = 1.0;
    qreal opacity = 1.0;
};

void setBranchGeometry(ProjectedNode &node)
{
    const QPointF delta = node.position - node.branchStart;
    node.branchControl1 = node.branchStart + delta * 0.34;
    node.branchControl2 = node.branchStart + delta * 0.72;
}

qreal laneLateralPosition(int lane, qreal spacing)
{
    const qreal direction = lane % 2 == 0 ? -1.0 : 1.0;
    const int band = lane / 2 + 1;
    return direction * band * spacing;
}

qreal fogOpacity(qreal cameraDepth, const PerspectiveProfile &profile)
{
    const qreal fogProgress = qBound(
        0.0,
        (cameraDepth - profile.fogStartDepth)
            / (profile.fogEndDepth - profile.fogStartDepth),
        1.0);
    return 1.0
        - fogProgress * (1.0 - profile.minimumFogOpacity);
}

ScreenPoint projectPoint(
    const WorldPoint &point,
    const ViewState &view,
    const PerspectiveProfile &profile)
{
    const qreal width = view.viewport.width() > 0.0
        ? view.viewport.width() : profile.fallbackViewportWidth;
    const qreal height = view.viewport.height() > 0.0
        ? view.viewport.height() : profile.fallbackViewportHeight;
    const qreal centerX = width * profile.centerXRatio;
    const qreal horizonY = height * profile.horizonYRatio;
    const qreal nearY = height * profile.nearYRatio;
    const qreal cameraDepth = qMax(
        profile.minimumCameraDepth,
        profile.cameraClearance + point.depth - view.travelDepth);
    const qreal scale = profile.focalDepth
        / (profile.focalDepth + cameraDepth);

    return {
        .position = {
            centerX + point.lateral * scale,
            horizonY + (nearY - horizonY) * scale
        },
        .scale = scale,
        .opacity = fogOpacity(cameraDepth, profile)
    };
}

ProjectedNode *findProjectedNode(
    QVector<ProjectedNode> &nodes, NodeId id)
{
    for (ProjectedNode &node : nodes) {
        if (node.id == id)
            return &node;
    }
    return nullptr;
}
}

QVector<ProjectedNode> PerspectiveProjection::project(
    const WorldLayout &world,
    const QVector<FileNode> &nodes,
    const ViewState &view) const
{
    QVector<ProjectedNode> result;
    result.reserve(world.nodes.size());
    const ThemeProfile &theme = ThemeRegistry::active();
    const PerspectiveProfile &profile = theme.perspective;
    const LabelStyle &labels = theme.labels;
    QHash<NodeId, WorldPoint> worldPoints;

    for (const WorldNodePlacement &placement : world.nodes) {
        const FileNode *entry = findNode(nodes, placement.id);
        if (!entry)
            continue;

        WorldPoint point;
        switch (placement.role) {
        case WorldNodeRole::ParentContext:
            point.depth = -1.0;
            break;
        case WorldNodeRole::Focus:
            break;
        case WorldNodeRole::DirectChild:
            point.lateral = laneLateralPosition(
                placement.lane, profile.laneSpacing);
            point.depth = placement.distanceAlongParent;
            break;
        case WorldNodeRole::Descendant: {
            const WorldPoint parent =
                worldPoints.value(placement.parentId);
            point.lateral = parent.lateral
                + (placement.siblingOrder
                   - (placement.siblingCount - 1) * 0.5)
                    * profile.descendantSpacing;
            point.depth = parent.depth + placement.distanceAlongParent;
            break;
        }
        }
        worldPoints.insert(placement.id, point);

        const ScreenPoint projected = projectPoint(point, view, profile);
        const qreal radius = radiusFor(
            *entry, placement.role == WorldNodeRole::Focus)
            * projected.scale;
        ProjectedNode node {
            .id = entry->id,
            .parentId = placement.parentId,
            .position = projected.position,
            .radius = radius,
            .parentContext =
                placement.role == WorldNodeRole::ParentContext,
            .hiddenChildCount = placement.hiddenChildCount,
            .corridorSlot = placement.lane,
            .depthLayer = placement.semanticDepth,
            .depthOpacity = projected.opacity
        };

        if (ProjectedNode *parent =
                findProjectedNode(result, placement.parentId)) {
            node.branchStart = parent->position;
        } else {
            node.branchStart = node.position
                + QPointF(0.0, qMax(50.0, 90.0 * projected.scale));
        }
        node.crownEnd = node.position;
        node.labelBounds = {
            node.position.x() - labels.boundsWidth * 0.5,
            node.position.y() + radius + labels.gap,
            labels.boundsWidth,
            labels.boundsHeight
        };
        setBranchGeometry(node);
        result.append(node);
    }

    return result;
}

const FileNode *PerspectiveProjection::findNode(
    const QVector<FileNode> &nodes, NodeId id)
{
    if (id == InvalidNodeId)
        return nullptr;
    for (const FileNode &node : nodes) {
        if (node.id == id)
            return &node;
    }
    return nullptr;
}

qreal PerspectiveProjection::radiusFor(
    const FileNode &node, bool focused)
{
    if (focused)
        return 26.0;
    return node.isDirectory() ? 21.0 : 13.0;
}
