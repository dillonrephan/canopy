#include "WorldLayoutEngine.h"

#include "DirectoryPresentationPolicy.h"
#include "ThemeRegistry.h"

#include <QSet>

#include <algorithm>

WorldLayout WorldLayoutEngine::layout(
    const QVector<FileNode> &nodes, NodeId focusedId) const
{
    WorldLayout result {.focusedId = focusedId};
    const FileNode *focused = findNode(nodes, focusedId);
    if (!focused)
        return result;

    if (const FileNode *parent = findNode(nodes, focused->parentId)) {
        result.nodes.append({
            .id = parent->id,
            .parentId = InvalidNodeId,
            .role = WorldNodeRole::ParentContext,
            .semanticDepth = -1,
            .distanceAlongParent = -1.0
        });
    }

    const DirectoryPresentation presentation =
        DirectoryPresentationPolicy::evaluate(*focused);
    result.nodes.append({
        .id = focused->id,
        .parentId = focused->parentId,
        .role = WorldNodeRole::Focus,
        .semanticDepth = 0,
        .hiddenChildCount = presentation.hiddenChildCount
    });

    QVector<const FileNode *> visibleChildren;
    visibleChildren.reserve(presentation.visibleChildCount);
    for (qsizetype i = 0; i < presentation.visibleChildCount; ++i) {
        const FileNode *child = findNode(nodes, focused->children.at(i));
        if (child)
            visibleChildren.append(child);
    }

    const QVector<int> assignedSlots = corridorSlots(visibleChildren);
    for (qsizetype i = 0; i < visibleChildren.size(); ++i) {
        const FileNode *child = visibleChildren.at(i);
        const int lane = assignedSlots.at(i);
        result.nodes.append({
            .id = child->id,
            .parentId = focused->id,
            .role = WorldNodeRole::DirectChild,
            .siblingOrder = static_cast<int>(i),
            .siblingCount = static_cast<int>(visibleChildren.size()),
            .lane = lane,
            .semanticDepth = 1,
            .distanceAlongParent = lane / 2.0 + 1.0
        });
    }

    const int foregroundCount =
        ThemeRegistry::active().presentation.foregroundCorridorCount;
    if (visibleChildren.size() <= foregroundCount) {
        for (const FileNode *child : visibleChildren) {
            if (!child->isDirectory() || child->children.isEmpty())
                continue;

            const qsizetype shown =
                qMin<qsizetype>(child->children.size(), 4);
            for (qsizetype i = 0; i < shown; ++i) {
                const FileNode *grandchild =
                    findNode(nodes, child->children.at(i));
                if (!grandchild)
                    continue;
                result.nodes.append({
                    .id = grandchild->id,
                    .parentId = child->id,
                    .role = WorldNodeRole::Descendant,
                    .siblingOrder = static_cast<int>(i),
                    .siblingCount = static_cast<int>(shown),
                    .semanticDepth = 2,
                    .distanceAlongParent = 1.0
                });
            }
        }
    }

    return result;
}

const FileNode *WorldLayoutEngine::findNode(
    const QVector<FileNode> &nodes, NodeId id)
{
    if (id == InvalidNodeId)
        return nullptr;
    for (const FileNode &node : nodes) {
        if (node.id == id)
            return &node;
    }
    return nullptr;
}

QVector<int> WorldLayoutEngine::corridorSlots(
    const QVector<const FileNode *> &children) const
{
    const PresentationProfile &presentation =
        ThemeRegistry::active().presentation;
    const int corridorCount =
        presentation.foregroundCorridorCount + presentation.fogHintCount;
    const int foregroundCount = presentation.foregroundCorridorCount;

    QVector<const FileNode *> allocationOrder = children;
    std::sort(
        allocationOrder.begin(), allocationOrder.end(),
        [](const FileNode *left, const FileNode *right) {
            return left->id < right->id;
        });

    QSet<int> usedSlots;
    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && existing < corridorCount
            && !usedSlots.contains(existing)) {
            usedSlots.insert(existing);
        }
    }

    for (const FileNode *child : allocationOrder) {
        const int existing = m_corridorSlots.value(child->id, -1);
        if (existing >= 0 && usedSlots.contains(existing))
            continue;

        const int preferred = static_cast<int>(
            qHash(child->path) % static_cast<size_t>(foregroundCount));
        for (int distance = 0; distance < corridorCount; ++distance) {
            const int layer = distance / foregroundCount;
            const int offset = distance % foregroundCount;
            const int candidate = layer * foregroundCount
                + (preferred + offset) % foregroundCount;
            if (usedSlots.contains(candidate))
                continue;
            m_corridorSlots.insert(child->id, candidate);
            usedSlots.insert(candidate);
            break;
        }
    }

    QVector<int> result;
    result.reserve(children.size());
    for (const FileNode *child : children)
        result.append(m_corridorSlots.value(child->id, 0));
    return result;
}
