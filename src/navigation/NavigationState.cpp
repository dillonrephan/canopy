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
