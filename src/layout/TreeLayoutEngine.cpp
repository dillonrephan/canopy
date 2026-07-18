#include "TreeLayoutEngine.h"

#include <QtMath>

namespace {
constexpr QPointF kFocusPosition {420.0, 500.0};
constexpr qreal kLabelWidth = 164.0;
constexpr qreal kLabelHeight = 34.0;
constexpr qreal kLabelGap = 8.0;
constexpr qsizetype kWideColumns = 4;
constexpr qreal kWideHorizontalSpacing = 185.0;
constexpr qreal kWideVerticalSpacing = 86.0;
constexpr qreal kWideFirstRowY = 340.0;

LayoutNode makeLayoutNode(NodeId id,
                          NodeId parentId,
                          QPointF position,
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
    return result;
}

QVector<QPointF> childPositions(qsizetype count)
{
    QVector<QPointF> positions;
    positions.reserve(count);
    if (count <= 0)
        return positions;

    if (count <= 8) {
        const qreal span = qMin(660.0, qMax(120.0, (count - 1) * 125.0));
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

    if (const FileNode *parent = findNode(nodes, focused->parentId)) {
        result.append(makeLayoutNode(parent->id, InvalidNodeId,
                                     {420.0, 635.0}, 17.0, true));
    }

    result.append(makeLayoutNode(focused->id, focused->parentId, kFocusPosition,
                                 radiusFor(*focused, true)));

    const QVector<QPointF> positions = childPositions(focused->children.size());
    for (qsizetype i = 0; i < focused->children.size(); ++i) {
        const FileNode *child = findNode(nodes, focused->children.at(i));
        if (!child)
            continue;
        result.append(makeLayoutNode(child->id, focused->id, positions.at(i),
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
