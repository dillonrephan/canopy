#include "LabelVisibilityPolicy.h"

QSet<NodeId> LabelVisibilityPolicy::visibleLabels(
    const QVector<LayoutNode> &layout,
    NodeId,
    NodeId selectedId,
    NodeId hoveredId,
    qreal zoom)
{
    QSet<NodeId> result;
    for (const LayoutNode &node : layout) {
        if (zoom >= 0.72 || node.id == selectedId || node.id == hoveredId)
            result.insert(node.id);
    }
    return result;
}
