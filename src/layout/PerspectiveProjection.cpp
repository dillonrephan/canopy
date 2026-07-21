#include "PerspectiveProjection.h"

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
    QVector<const WorldNodePlacement *> directChildren;
    for (const WorldNodePlacement &placement : world.nodes) {
        if (placement.role == WorldNodeRole::DirectChild)
            directChildren.append(&placement);
    }
    std::sort(
        directChildren.begin(), directChildren.end(),
        [](const WorldNodePlacement *left,
           const WorldNodePlacement *right) {
            if (left->distanceAlongParent
                != right->distanceAlongParent) {
                return left->distanceAlongParent
                    < right->distanceAlongParent;
            }
            return left->id < right->id;
        });

    QSet<NodeId> includedDirectChildren;
    QSet<NodeId> fogHintIds;
    const qsizetype foregroundCount = qMin<qsizetype>(
        profile.foregroundCorridorCount, directChildren.size());
    const qsizetype visibleCount = qMin<qsizetype>(
        directChildren.size(),
        profile.foregroundCorridorCount + profile.fogHintCount);
    for (qsizetype i = 0; i < visibleCount; ++i) {
        includedDirectChildren.insert(directChildren.at(i)->id);
        if (i >= foregroundCount)
            fogHintIds.insert(directChildren.at(i)->id);
    }

    for (const WorldNodePlacement &placement : world.nodes) {
        if (placement.role == WorldNodeRole::DirectChild
            && !includedDirectChildren.contains(placement.id)) {
            continue;
        }
        if (placement.role == WorldNodeRole::Descendant
            && !includedDirectChildren.contains(placement.parentId)) {
            continue;
        }
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
            .depthOpacity = fogHintIds.contains(placement.id)
                ? qMin(projected.opacity, profile.fogHintOpacity)
                : projected.opacity,
            .cameraDepth = projected.cameraDepth,
            .apparentScale = projected.scale,
            .fogHint = fogHintIds.contains(placement.id)
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
