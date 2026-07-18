#include "TreeLayoutEngine.h"

#include <QtMath>

namespace {
constexpr QPointF kFocusPosition {420.0, 500.0};
constexpr qreal kLabelWidth = 164.0;
constexpr qreal kLabelHeight = 34.0;
constexpr qreal kLabelGap = 8.0;
constexpr qsizetype kFanChildLimit = 5;
constexpr qreal kFanHorizontalSpacing = 174.0;
constexpr qreal kFanMaximumSpan = 700.0;
constexpr qsizetype kWideColumns = 4;
constexpr qreal kWideHorizontalSpacing = 185.0;
constexpr qreal kWideVerticalSpacing = 86.0;
constexpr qreal kWideFirstRowY = 340.0;

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
    setBranchGeometry(result);
    return result;
}

QVector<QPointF> childPositions(qsizetype count)
{
    QVector<QPointF> positions;
    positions.reserve(count);
    if (count <= 0)
        return positions;

    if (count <= kFanChildLimit) {
        const qreal span = qMin(kFanMaximumSpan,
                                qMax(120.0, (count - 1) * kFanHorizontalSpacing));
        for (qsizetype i = 0; i < count; ++i) {
            const qreal normalized = count == 1 ? 0.0
                : (static_cast<qreal>(i) / (count - 1)) * 2.0 - 1.0;
            positions.append({kFocusPosition.x() + normalized * span * 0.5,
                              320.0 + qAbs(normalized) * 38.0});
        }
        return positions;
    }

    for (qsizetype i = 0; i < count; ++i) {
        const qsizetype row = i / kWideColumns;
        const qsizetype rowStart = row * kWideColumns;
        const qsizetype itemsInRow = qMin(kWideColumns, count - rowStart);
        const qsizetype column = i - rowStart;
        const qreal centeredColumn = column - (itemsInRow - 1) * 0.5;
        positions.append({kFocusPosition.x()
                              + centeredColumn * kWideHorizontalSpacing,
                          kWideFirstRowY - row * kWideVerticalSpacing});
    }
    return positions;
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

    const QVector<QPointF> positions = childPositions(focused->children.size());
    for (qsizetype i = 0; i < focused->children.size(); ++i) {
        const FileNode *child = findNode(nodes, focused->children.at(i));
        if (!child)
            continue;
        result.append(makeLayoutNode(child->id, focused->id, positions.at(i),
                                     kFocusPosition,
                                     radiusFor(*child, false)));
    }

    if (focused->children.size() <= 8) {
        for (qsizetype childIndex = 0; childIndex < focused->children.size(); ++childIndex) {
            const FileNode *child = findNode(nodes, focused->children.at(childIndex));
            if (!child || !child->isDirectory() || child->children.isEmpty())
                continue;

            const qsizetype shown = qMin<qsizetype>(child->children.size(), 4);
            const QPointF anchor = positions.at(childIndex);
            for (qsizetype i = 0; i < shown; ++i) {
                const FileNode *grandchild = findNode(nodes, child->children.at(i));
                if (!grandchild)
                    continue;
                const qreal offset = (i - (shown - 1) * 0.5) * 48.0;
                result.append(makeLayoutNode(
                    grandchild->id,
                    child->id,
                    {anchor.x() + offset, anchor.y() - 120.0 - qAbs(offset) * 0.08},
                    anchor,
                    radiusFor(*grandchild, false) * 0.88));
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
