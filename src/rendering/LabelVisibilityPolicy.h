#pragma once

#include "TreeLayoutEngine.h"

#include <QSet>
#include <QVector>

class LabelVisibilityPolicy
{
public:
    [[nodiscard]] static qreal depthOpacity(qreal baseOpacity, qreal zoom);
    [[nodiscard]] static QSet<NodeId> visibleLabels(
        const QVector<LayoutNode> &layout,
        NodeId focusedId,
        NodeId selectedId,
        NodeId hoveredId,
        qreal zoom);
};
