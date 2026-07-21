#include "PerspectiveProjection.h"

#include "PerspectivePresentationPolicy.h"
#include "ThemeRegistry.h"

#include <QHash>
#include <QPainterPathStroker>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace {
struct WorldPoint {
    qreal lateral = 0.0;
    qreal depth = 0.0;
};

struct ScreenPoint {
    QPointF position;
    qreal cameraDepth = 0.0;
    qreal scale = 1.0;
    qreal opacity = 1.0;
    bool visible = true;
};

void setBranchGeometry(ProjectedNode &node)
{
    const QPointF delta = node.position - node.branchStart;
    node.branchControl1 = node.branchStart + delta * 0.34;
    node.branchControl2 = node.branchStart + delta * 0.72;
}

QPainterPath taperedSurface(
    const ProjectedNode &node,
    qreal startWidth,
    qreal endWidth)
{
    const QPointF delta = node.position - node.branchStart;
    const qreal length =
        qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    if (length <= 0.001) {
        QPainterPath dot;
        const qreal radius = qMax(startWidth, endWidth) * 0.5;
        dot.addEllipse(node.position, radius, radius);
        return dot;
    }

    const QPointF normal(-delta.y() / length, delta.x() / length);
    const qreal startHalf = startWidth * 0.5;
    const qreal endHalf = endWidth * 0.5;
    const qreal control1Half =
        startHalf + (endHalf - startHalf) * 0.34;
    const qreal control2Half =
        startHalf + (endHalf - startHalf) * 0.72;

    QPainterPath surface;
    surface.moveTo(node.branchStart + normal * startHalf);
    surface.cubicTo(
        node.branchControl1 + normal * control1Half,
        node.branchControl2 + normal * control2Half,
        node.position + normal * endHalf);
    surface.lineTo(node.position - normal * endHalf);
    surface.cubicTo(
        node.branchControl2 - normal * control2Half,
        node.branchControl1 - normal * control1Half,
        node.branchStart - normal * startHalf);
    surface.closeSubpath();
    return surface;
}

QPainterPath paddedHitArea(
    const QPainterPath &surface, qreal padding)
{
    QPainterPathStroker stroker;
    stroker.setWidth(padding * 2.0);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return surface.united(stroker.createStroke(surface));
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
    const qreal rawCameraDepth =
        profile.cameraClearance + point.depth - view.travelDepth;
    if (rawCameraDepth <= 0.0)
        return {.cameraDepth = rawCameraDepth, .visible = false};
    const qreal cameraDepth =
        qMax(profile.minimumCameraDepth, rawCameraDepth);
    const qreal scale = profile.focalDepth
        / (profile.focalDepth + cameraDepth);

    return {
        .position = {
            centerX + point.lateral * scale,
            horizonY + (nearY - horizonY) * scale
        },
        .cameraDepth = cameraDepth,
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
    const PerspectiveDirectChildWindow window =
        PerspectivePresentationPolicy::select(world, view, profile);

    for (const WorldNodePlacement &placement : world.nodes) {
        if (placement.role == WorldNodeRole::DirectChild
            && !window.isForeground(placement.id)
            && !window.isFogHint(placement.id)) {
            continue;
        }
        if (placement.role == WorldNodeRole::Descendant)
            continue;
        const FileNode *entry = findNode(nodes, placement.id);
        if (!entry)
            continue;

        WorldPoint point;
        switch (placement.role) {
        case WorldNodeRole::ParentContext:
            // Keep back context nearer than the focused trunk without placing
            // it on the camera plane, where it would be deterministically
            // culled. Deriving this from camera clearance keeps the placement
            // coherent if the profile changes its near-camera scale.
            point.depth = -profile.cameraClearance * 0.5;
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

        ViewState projectionView = view;
        if (placement.role == WorldNodeRole::ParentContext
            || placement.role == WorldNodeRole::Focus) {
            projectionView.travelDepth = 0.0;
        }
        const ScreenPoint projected = projectPoint(
            point, projectionView, profile);
        if (!projected.visible)
            continue;
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
            .depthOpacity = window.isFogHint(placement.id)
                ? qMin(projected.opacity, profile.fogHintOpacity)
                : projected.opacity,
            .cameraDepth = projected.cameraDepth,
            .apparentScale = projected.scale,
            .fogHint = window.isFogHint(placement.id)
        };

        qreal parentScale = 1.0;
        if (ProjectedNode *parent =
                findProjectedNode(result, placement.parentId)) {
            node.branchStart = parent->position;
            parentScale = parent->apparentScale;
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
        const bool focused = placement.role == WorldNodeRole::Focus;
        const qreal corridorWidth = entry->isDirectory()
            ? profile.directoryCorridorWidth
            : profile.fileCorridorWidth;
        node.branchStartWidth = focused
            ? profile.focusedTrunkNearWidth
            : corridorWidth * parentScale;
        node.branchEndWidth = focused
            ? profile.focusedTrunkFarWidth * projected.scale
            : corridorWidth * profile.corridorTipRatio
                * projected.scale;
        node.branchSurface = taperedSurface(
            node, node.branchStartWidth, node.branchEndWidth);
        node.hitArea = paddedHitArea(
            node.branchSurface, profile.hitPadding);
        result.append(node);
    }

    if (ProjectedNode *focus =
            findProjectedNode(result, world.focusedId)) {
        if (const FileNode *focusedEntry =
                findNode(nodes, world.focusedId)) {
            const quint64 projectedDirectChildCount =
                static_cast<quint64>(std::count_if(
                    result.cbegin(), result.cend(),
                    [focusedId = world.focusedId](
                        const ProjectedNode &node) {
                        return node.parentId == focusedId
                            && node.depthLayer == 1;
                    }));
            focus->hiddenChildCount =
                PerspectivePresentationPolicy::hiddenDirectChildCount(
                    focusedEntry->totalChildCount,
                    window.materializedDirectChildCount,
                    projectedDirectChildCount);
        }
    }

    // Projected surfaces are returned in painter order. The renderer reverses
    // this order for hit testing so overlapping near growth wins input.
    std::stable_sort(
        result.begin(), result.end(),
        [](const ProjectedNode &left, const ProjectedNode &right) {
            return left.cameraDepth > right.cameraDepth;
        });

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
