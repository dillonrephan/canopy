#pragma once

#include "FileNode.h"

#include <QPointF>
#include <QRectF>
#include <QtTypes>

struct ProjectedNode {
    NodeId id = InvalidNodeId;
    NodeId parentId = InvalidNodeId;
    QPointF position;
    qreal radius = 14.0;
    bool parentContext = false;
    QRectF labelBounds;
    QPointF branchStart;
    QPointF branchControl1;
    QPointF branchControl2;
    QPointF crownEnd;
    quint64 hiddenChildCount = 0;
    int corridorSlot = -1;
    int depthLayer = 0;
    qreal depthOpacity = 1.0;
    qreal cameraDepth = 0.0;
    qreal apparentScale = 1.0;
    bool fogHint = false;
    bool followsParentPosition = true;
};

// Transitional name retained while renderer code moves to projection language.
using LayoutNode = ProjectedNode;
