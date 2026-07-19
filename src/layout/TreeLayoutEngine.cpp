#include "TreeLayoutEngine.h"

#include "DirectoryPresentationPolicy.h"

#include <QSet>
#include <QtMath>

#include <algorithm>

namespace {
constexpr QPointF kFocusPosition {420.0, 500.0};
constexpr qreal kLabelWidth = 164.0;
constexpr qreal kLabelHeight = 34.0;
constexpr qreal kLabelGap = 8.0;
constexpr qsizetype kCorridorCount = 16;

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
    LayoutNode result {id, parentId, position, radius, parentContext};
    result.labelBounds = {
        position.x() - kLabelWidth * 0.5,
        position.y() + radius + kLabelGap,
        kLabelWidth,
        kLabelHeight
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
    slot = qBound(0, slot, static_cast<int>(kCorridorCount - 1));
    const int side = slot % 2 == 0 ? -1 : 1;
    const int pair = slot / 2;
    const int lateralBand = pair % 4;
    const int depthBand = pair / 4;
    const qreal distance = 150.0 + lateralBand * 65.0;
    const qreal endpointY =
        (depthBand == 0 ? 300.0 : 130.0) + lateralBand * 42.0;
    const qreal attachmentY =
        kFocusPosition.y() - 14.0 - pair * 15.0 - (side > 0 ? 6.0 : 0.0);
    const qreal opacity = depthBand == 0
        ? 1.0 : 0.68 - (pair - 4) * 0.12;
    return {
        .endpoint = {kFocusPosition.x() + side * distance, endpointY},
        .attachment = {kFocusPosition.x(), attachmentY},
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

    QPointF focusedBranchStart {420.0, 650.0};
    if (const FileNode *parent = findNode(nodes, focused->parentId)) {
        result.append(makeLayoutNode(parent->id, InvalidNodeId,
                                     {420.0, 635.0}, {420.0, 710.0},
                                     17.0, true));
        focusedBranchStart = {420.0, 635.0};
    }

    result.append(makeLayoutNode(focused->id, focused->parentId, kFocusPosition,
                                 focusedBranchStart,
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
    qreal highestAttachment = kFocusPosition.y();
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
        {kFocusPosition.x(), highestAttachment};

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
    QVector<const FileNode *> allocationOrder = children;
    std::sort(
        allocationOrder.begin(), allocationOrder.end(),
        [](const FileNode *left, const FileNode *right) {
            return left->id < right->id;
        });

    QSet<int> usedSlots;
    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && existing < kCorridorCount
            && !usedSlots.contains(existing)) {
            usedSlots.insert(existing);
        }
    }

    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && usedSlots.contains(existing))
            continue;

        constexpr int primaryCorridorCount =
            static_cast<int>(kCorridorCount / 2);
        const int preferred = static_cast<int>(
            qHash(child->path)
            % static_cast<size_t>(primaryCorridorCount));
        for (int distance = 0; distance < kCorridorCount; ++distance) {
            const int layer = distance / primaryCorridorCount;
            const int offset = distance % primaryCorridorCount;
            const int candidate = layer * primaryCorridorCount
                + (preferred + offset) % primaryCorridorCount;
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
