#include "TreeLayoutEngine.h"

#include "DirectoryPresentationPolicy.h"
#include "ThemeRegistry.h"

#include <QSet>
#include <QtMath>

#include <algorithm>

namespace {
void setBranchGeometry(LayoutNode &node)
{
    const QPointF delta = node.position - node.branchStart;
    const qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const qreal direction = node.id % 2 == 0 ? -1.0 : 1.0;
    const qreal bend = direction * qMin(18.0, 6.0 + qAbs(delta.x()) * 0.05);
    const QPointF normal = length > 0.0
        ? QPointF(-delta.y() / length, delta.x() / length)
        : QPointF();
    const QPointF offset = normal * bend;

    node.branchControl1 = node.branchStart + delta * 0.32 + offset;
    node.branchControl2 = node.branchStart + delta * 0.72 - offset * 0.35;
}

LayoutNode makeLayoutNode(NodeId id,
                          NodeId parentId,
                          QPointF position,
                          QPointF branchStart,
                          qreal radius,
                          bool parentContext = false)
{
    const LabelStyle &labels = ThemeRegistry::active().labels;
    LayoutNode result {id, parentId, position, radius, parentContext};
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

CorridorGeometry corridorGeometry(int slot)
{
    const PlanarLayoutProfile &planar = ThemeRegistry::active().planar;
    slot = qBound(0, slot, planar.corridorCount - 1);
    const int side = slot % 2 == 0 ? -1 : 1;
    const int pair = slot / 2;
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
}

QVector<LayoutNode> TreeLayoutEngine::layout(const QVector<FileNode> &nodes,
                                              NodeId focusedId) const
{
    QVector<LayoutNode> result;
    const FileNode *focused = findNode(nodes, focusedId);
    if (!focused)
        return result;
    const PlanarLayoutProfile &planar = ThemeRegistry::active().planar;

    QPointF focusedBranchStart = planar.rootBranchStart;
    if (const FileNode *parent = findNode(nodes, focused->parentId)) {
        result.append(makeLayoutNode(parent->id, InvalidNodeId,
                                     planar.parentPosition,
                                     planar.parentBranchStart,
                                     planar.parentRadius, true));
        focusedBranchStart = planar.parentPosition;
    }

    result.append(makeLayoutNode(focused->id, focused->parentId,
                                 planar.focusPosition, focusedBranchStart,
                                 radiusFor(*focused, true)));

    const DirectoryPresentation presentation =
        DirectoryPresentationPolicy::evaluate(*focused);
    result.last().hiddenChildCount = presentation.hiddenChildCount;
    const qsizetype visibleChildCount = presentation.visibleChildCount;
    QVector<const FileNode *> visibleChildren;
    visibleChildren.reserve(visibleChildCount);
    for (qsizetype i = 0; i < visibleChildCount; ++i) {
        const FileNode *child = findNode(nodes, focused->children.at(i));
        if (child)
            visibleChildren.append(child);
    }

    const QVector<int> assignedSlots = corridorSlots(visibleChildren);
    qreal highestAttachment = planar.focusPosition.y();
    QHash<NodeId, QPointF> childPositions;
    for (qsizetype i = 0; i < visibleChildren.size(); ++i) {
        const FileNode *child = visibleChildren.at(i);
        const int slot = assignedSlots.at(i);
        const CorridorGeometry corridor = corridorGeometry(slot);
        LayoutNode layout = makeLayoutNode(
            child->id,
            focused->id,
            corridor.endpoint,
            corridor.attachment,
            radiusFor(*child, false));
        layout.corridorSlot = slot;
        layout.depthLayer = corridor.depthLayer;
        layout.depthOpacity = corridor.opacity;
        layout.followsParentPosition = false;
        result.append(layout);
        childPositions.insert(child->id, corridor.endpoint);
        highestAttachment = qMin(highestAttachment, corridor.attachment.y());
    }
    result[findNode(nodes, focused->parentId) ? 1 : 0].crownEnd =
        {planar.focusPosition.x(), highestAttachment};

    if (visibleChildCount <= 8) {
        for (const FileNode *child : visibleChildren) {
            if (!child || !child->isDirectory() || child->children.isEmpty())
                continue;

            const qsizetype shown = qMin<qsizetype>(child->children.size(), 4);
            const QPointF anchor = childPositions.value(child->id);
            for (qsizetype i = 0; i < shown; ++i) {
                const FileNode *grandchild = findNode(nodes, child->children.at(i));
                if (!grandchild)
                    continue;
                const qreal offset = (i - (shown - 1) * 0.5) * 48.0;
                LayoutNode grandchildLayout = makeLayoutNode(
                    grandchild->id,
                    child->id,
                    {anchor.x() + offset, anchor.y() - 120.0 - qAbs(offset) * 0.08},
                    anchor,
                    radiusFor(*grandchild, false) * 0.88);
                grandchildLayout.depthLayer = 2;
                grandchildLayout.depthOpacity = 0.74;
                result.append(grandchildLayout);
            }
        }
    }

    return result;
}

const FileNode *TreeLayoutEngine::findNode(const QVector<FileNode> &nodes, NodeId id)
{
    if (id == InvalidNodeId)
        return nullptr;
    for (const FileNode &node : nodes) {
        if (node.id == id)
            return &node;
    }
    return nullptr;
}

qreal TreeLayoutEngine::radiusFor(const FileNode &node, bool focused)
{
    if (focused)
        return 26.0;
    return node.isDirectory() ? 21.0 : 13.0;
}

QVector<int> TreeLayoutEngine::corridorSlots(
    const QVector<const FileNode *> &children) const
{
    const PlanarLayoutProfile &planar = ThemeRegistry::active().planar;
    QVector<const FileNode *> allocationOrder = children;
    std::sort(
        allocationOrder.begin(), allocationOrder.end(),
        [](const FileNode *left, const FileNode *right) {
            return left->id < right->id;
        });

    QSet<int> usedSlots;
    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && existing < planar.corridorCount
            && !usedSlots.contains(existing)) {
            usedSlots.insert(existing);
        }
    }

    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && usedSlots.contains(existing))
            continue;

        const int preferred = static_cast<int>(
            qHash(child->path)
            % static_cast<size_t>(planar.primaryCorridorCount));
        for (int distance = 0; distance < planar.corridorCount; ++distance) {
            const int layer = distance / planar.primaryCorridorCount;
            const int offset = distance % planar.primaryCorridorCount;
            const int candidate = layer * planar.primaryCorridorCount
                + (preferred + offset) % planar.primaryCorridorCount;
            if (usedSlots.contains(candidate))
                continue;
            m_corridorSlots.insert(child->id, candidate);
            usedSlots.insert(candidate);
            break;
        }
    }

    QVector<int> result;
    result.reserve(children.size());
    for (const FileNode *child : children)
        result.append(m_corridorSlots.value(child->id, 0));
    return result;
}
