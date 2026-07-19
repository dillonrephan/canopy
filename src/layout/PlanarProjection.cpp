#include "PlanarProjection.h"

#include "ThemeRegistry.h"

#include <QtMath>

namespace {
void setBranchGeometry(ProjectedNode &node)
{
    const QPointF delta = node.position - node.branchStart;
    const qreal length =
        qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const qreal direction = node.id % 2 == 0 ? -1.0 : 1.0;
    const qreal bend =
        direction * qMin(18.0, 6.0 + qAbs(delta.x()) * 0.05);
    const QPointF normal = length > 0.0
        ? QPointF(-delta.y() / length, delta.x() / length)
        : QPointF();
    const QPointF offset = normal * bend;

    node.branchControl1 = node.branchStart + delta * 0.32 + offset;
    node.branchControl2 =
        node.branchStart + delta * 0.72 - offset * 0.35;
}

ProjectedNode makeProjectedNode(
    NodeId id,
    NodeId parentId,
    QPointF position,
    QPointF branchStart,
    qreal radius,
    bool parentContext = false)
{
    const LabelStyle &labels = ThemeRegistry::active().labels;
    ProjectedNode result {
        id, parentId, position, radius, parentContext};
    result.labelBounds = {
        position.x() - labels.boundsWidth * 0.5,
        position.y() + radius + labels.gap,
        labels.boundsWidth,
        labels.boundsHeight
    };
    result.branchStart = branchStart;
    result.crownEnd = position;
    setBranchGeometry(result);
    return result;
}

struct CorridorGeometry {
    QPointF endpoint;
    QPointF attachment;
    int depthLayer = 1;
    qreal opacity = 1.0;
};

CorridorGeometry corridorGeometry(int lane)
{
    const PlanarLayoutProfile &planar = ThemeRegistry::active().planar;
    lane = qBound(0, lane, planar.corridorCount - 1);
    const int side = lane % 2 == 0 ? -1 : 1;
    const int pair = lane / 2;
    const int lateralBandCount = planar.primaryCorridorCount / 2;
    const int lateralBand = pair % lateralBandCount;
    const int depthBand = pair / lateralBandCount;
    const qreal distance = planar.initialLateralDistance
        + lateralBand * planar.lateralDistanceStep;
    const qreal endpointY =
        (depthBand == 0
             ? planar.frontEndpointY : planar.rearEndpointY)
        + lateralBand * planar.endpointYStep;
    const qreal attachmentY =
        planar.focusPosition.y() - planar.firstAttachmentOffset
        - pair * planar.attachmentStep
        - (side > 0 ? planar.rightAttachmentOffset : 0.0);
    const qreal opacity = depthBand == 0
        ? 1.0
        : planar.rearInitialOpacity
            - (pair - lateralBandCount) * planar.rearOpacityStep;
    return {
        .endpoint = {
            planar.focusPosition.x() + side * distance, endpointY},
        .attachment = {planar.focusPosition.x(), attachmentY},
        .depthLayer = depthBand + 1,
        .opacity = opacity
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

QVector<ProjectedNode> PlanarProjection::project(
    const WorldLayout &world,
    const QVector<FileNode> &nodes,
    const ViewState &view) const
{
    Q_UNUSED(view)
    QVector<ProjectedNode> result;
    result.reserve(world.nodes.size());
    const PlanarLayoutProfile &planar = ThemeRegistry::active().planar;
    qreal highestAttachment = planar.focusPosition.y();

    for (const WorldNodePlacement &placement : world.nodes) {
        const FileNode *entry = findNode(nodes, placement.id);
        if (!entry)
            continue;

        switch (placement.role) {
        case WorldNodeRole::ParentContext:
            result.append(makeProjectedNode(
                entry->id,
                InvalidNodeId,
                planar.parentPosition,
                planar.parentBranchStart,
                planar.parentRadius,
                true));
            break;
        case WorldNodeRole::Focus: {
            const QPointF branchStart =
                findNode(nodes, entry->parentId)
                ? planar.parentPosition : planar.rootBranchStart;
            ProjectedNode focus = makeProjectedNode(
                entry->id,
                entry->parentId,
                planar.focusPosition,
                branchStart,
                radiusFor(*entry, true));
            focus.hiddenChildCount = placement.hiddenChildCount;
            result.append(focus);
            break;
        }
        case WorldNodeRole::DirectChild: {
            const CorridorGeometry corridor =
                corridorGeometry(placement.lane);
            ProjectedNode child = makeProjectedNode(
                entry->id,
                placement.parentId,
                corridor.endpoint,
                corridor.attachment,
                radiusFor(*entry, false));
            child.corridorSlot = placement.lane;
            child.depthLayer = corridor.depthLayer;
            child.depthOpacity = corridor.opacity;
            child.followsParentPosition = false;
            result.append(child);
            highestAttachment =
                qMin(highestAttachment, corridor.attachment.y());
            break;
        }
        case WorldNodeRole::Descendant: {
            const ProjectedNode *parent =
                findProjectedNode(result, placement.parentId);
            if (!parent)
                break;
            const qreal offset =
                (placement.siblingOrder
                 - (placement.siblingCount - 1) * 0.5) * 48.0;
            ProjectedNode descendant = makeProjectedNode(
                entry->id,
                placement.parentId,
                {
                    parent->position.x() + offset,
                    parent->position.y() - 120.0
                        - qAbs(offset) * 0.08
                },
                parent->position,
                radiusFor(*entry, false) * 0.88);
            descendant.depthLayer = 2;
            descendant.depthOpacity = 0.74;
            result.append(descendant);
            break;
        }
        }
    }

    if (ProjectedNode *focus =
            findProjectedNode(result, world.focusedId)) {
        focus->crownEnd = {
            planar.focusPosition.x(), highestAttachment};
    }
    return result;
}

const FileNode *PlanarProjection::findNode(
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

qreal PlanarProjection::radiusFor(
    const FileNode &node, bool focused)
{
    if (focused)
        return 26.0;
    return node.isDirectory() ? 21.0 : 13.0;
}
