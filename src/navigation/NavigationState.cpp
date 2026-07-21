#include "NavigationState.h"

NavigationState::NavigationState(NodeId initialNode)
    : m_focusedId(initialNode)
    , m_selectedId(initialNode)
{
}

bool NavigationState::select(NodeId id)
{
    if (id == InvalidNodeId || id == m_selectedId)
        return false;
    m_selectedId = id;
    return true;
}

bool NavigationState::focus(NodeId id, bool addToHistory)
{
    if (id == InvalidNodeId || id == m_focusedId)
        return false;
    if (addToHistory)
        m_backHistory.append(m_focusedId);
    m_focusedId = id;
    m_selectedId = id;
    return true;
}

NodeId NavigationState::takeBackTarget()
{
    if (m_backHistory.isEmpty())
        return InvalidNodeId;
    return m_backHistory.takeLast();
}

NavigationReconciliation NavigationState::reconcileRemoved(
    const QSet<NodeId> &removedIds,
    NodeId fallbackId)
{
    const qsizetype previousHistorySize = m_backHistory.size();
    m_backHistory.removeIf([&removedIds](NodeId id) {
        return removedIds.contains(id);
    });

    NavigationReconciliation result;
    if (removedIds.contains(m_focusedId)) {
        result.focusChanged = m_focusedId != fallbackId;
        result.selectionChanged = m_selectedId != fallbackId;
        m_focusedId = fallbackId;
        m_selectedId = fallbackId;
    } else if (removedIds.contains(m_selectedId)) {
        result.selectionChanged = m_selectedId != m_focusedId;
        m_selectedId = m_focusedId;
    }
    if (!removedIds.isEmpty()) {
        m_backHistory.removeIf([this](NodeId id) {
            return id == m_focusedId;
        });
    }
    result.historyChanged = m_backHistory.size() != previousHistorySize;
    return result;
}
