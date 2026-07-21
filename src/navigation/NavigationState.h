#pragma once

#include "FileNode.h"

#include <QSet>
#include <QVector>

struct NavigationReconciliation {
    bool focusChanged = false;
    bool selectionChanged = false;
    bool historyChanged = false;
};

class NavigationState
{
public:
    explicit NavigationState(NodeId initialNode = InvalidNodeId);

    [[nodiscard]] NodeId focusedId() const noexcept { return m_focusedId; }
    [[nodiscard]] NodeId selectedId() const noexcept { return m_selectedId; }
    [[nodiscard]] bool canGoBack() const noexcept { return !m_backHistory.isEmpty(); }

    bool select(NodeId id);
    bool focus(NodeId id, bool addToHistory);
    [[nodiscard]] NodeId takeBackTarget();
    [[nodiscard]] NavigationReconciliation reconcileRemoved(
        const QSet<NodeId> &removedIds,
        NodeId fallbackId);

private:
    QVector<NodeId> m_backHistory;
    NodeId m_focusedId = InvalidNodeId;
    NodeId m_selectedId = InvalidNodeId;
};
