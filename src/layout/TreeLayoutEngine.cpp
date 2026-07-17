#include "TreeLayoutEngine.h"

#include <QtMath>

namespace {
constexpr QPointF kFocusPosition {420.0, 500.0};

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

    const qsizetype columns = (count + 1) / 2;
    const qreal spacing = columns <= 1 ? 0.0 : qMin(92.0, 700.0 / (columns - 1));
    for (qsizetype i = 0; i < count; ++i) {
        const qsizetype column = i / 2;
        const qsizetype row = i % 2;
        positions.append({kFocusPosition.x()
                              + (column - (columns - 1) * 0.5) * spacing,
                          row == 0 ? 330.0 : 235.0});
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
        result.append({parent->id, InvalidNodeId, {420.0, 635.0}, 17.0, true});
    }

    result.append({focused->id, focused->parentId, kFocusPosition,
                   radiusFor(*focused, true), false});

    const QVector<QPointF> positions = childPositions(focused->children.size());
    for (qsizetype i = 0; i < focused->children.size(); ++i) {
        const FileNode *child = findNode(nodes, focused->children.at(i));
        if (!child)
            continue;
        result.append({child->id, focused->id, positions.at(i),
                       radiusFor(*child, false), false});
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
                result.append({grandchild->id, child->id,
                               {anchor.x() + offset, anchor.y() - 120.0 - qAbs(offset) * 0.08},
                               radiusFor(*grandchild, false) * 0.88, false});
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
