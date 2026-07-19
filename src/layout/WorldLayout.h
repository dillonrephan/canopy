#pragma once

#include "FileNode.h"

#include <QVector>
#include <QtTypes>

enum class WorldNodeRole {
    ParentContext,
    Focus,
    DirectChild,
    Descendant
};

struct WorldNodePlacement {
    NodeId id = InvalidNodeId;
    NodeId parentId = InvalidNodeId;
    WorldNodeRole role = WorldNodeRole::DirectChild;
    int siblingOrder = 0;
    int siblingCount = 1;
    int lane = -1;
    int semanticDepth = 0;
    qreal distanceAlongParent = 0.0;
    quint64 hiddenChildCount = 0;
};

struct WorldLayout {
    NodeId focusedId = InvalidNodeId;
    QVector<WorldNodePlacement> nodes;
};
