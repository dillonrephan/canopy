#pragma once

#include "FileNode.h"

#include <QPointF>
#include <QRectF>
#include <QVector>

struct LayoutNode {
    NodeId id = InvalidNodeId;
    NodeId parentId = InvalidNodeId;
    QPointF position;
    qreal radius = 14.0;
    bool parentContext = false;
    QRectF labelBounds;
    QPointF branchStart;
    QPointF branchControl1;
    QPointF branchControl2;
    quint64 hiddenChildCount = 0;
};

class TreeLayoutEngine
{
public:
    [[nodiscard]] QVector<LayoutNode> layout(const QVector<FileNode> &nodes,
                                              NodeId focusedId) const;

private:
    [[nodiscard]] static const FileNode *findNode(const QVector<FileNode> &nodes,
                                                  NodeId id);
    [[nodiscard]] static qreal radiusFor(const FileNode &node, bool focused);
};
