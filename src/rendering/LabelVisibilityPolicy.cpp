#include "LabelVisibilityPolicy.h"

#include "ThemeRegistry.h"

#include <algorithm>

namespace {
struct LabelCandidate {
    const LayoutNode *node = nullptr;
    int priority = 0;
    bool emphasized = false;
};

LabelCandidate candidateFor(const LayoutNode &node,
                            NodeId focusedId,
                            NodeId selectedId,
                            NodeId hoveredId)
{
    if (node.id == hoveredId)
        return {&node, 0, true};
    if (node.id == selectedId)
        return {&node, 1, true};
    if (node.id == focusedId)
        return {&node, 2, true};
    if (node.parentId == focusedId)
        return {&node, 3, false};
    if (node.parentContext)
        return {&node, 4, false};
    return {&node, 5, false};
}

bool isEligible(const LabelCandidate &candidate, qreal zoom)
{
    const LabelStyle &labels = ThemeRegistry::active().labels;
    if (candidate.emphasized)
        return true;
    if (candidate.priority <= 4)
        return zoom >= labels.primaryRevealZoom;
    return zoom >= labels.descendantRevealZoom;
}

bool collidesWith(const QRectF &bounds, const QVector<QRectF> &occupied)
{
    return std::ranges::any_of(occupied, [&bounds](const QRectF &existing) {
        return bounds.intersects(existing);
    });
}
}

qreal LabelVisibilityPolicy::depthOpacity(qreal baseOpacity, qreal zoom)
{
    const qreal fogClearingZoom =
        ThemeRegistry::active().camera.fogClearingZoom;
    const qreal fogLift = qBound(
        0.0, (zoom - 1.0) / (fogClearingZoom - 1.0), 1.0);
    return baseOpacity + (1.0 - baseOpacity) * fogLift;
}

QSet<NodeId> LabelVisibilityPolicy::visibleLabels(
    const QVector<LayoutNode> &layout,
    NodeId focusedId,
    NodeId selectedId,
    NodeId hoveredId,
    qreal zoom)
{
    QVector<LabelCandidate> candidates;
    candidates.reserve(layout.size());
    for (const LayoutNode &node : layout)
        candidates.append(candidateFor(node, focusedId, selectedId, hoveredId));

    std::ranges::stable_sort(candidates, {}, &LabelCandidate::priority);

    QSet<NodeId> result;
    QVector<QRectF> occupied;
    occupied.reserve(candidates.size());
    for (const LabelCandidate &candidate : candidates) {
        if (!isEligible(candidate, zoom))
            continue;
        if (!candidate.emphasized
            && collidesWith(candidate.node->labelBounds, occupied)) {
            continue;
        }
        result.insert(candidate.node->id);
        occupied.append(candidate.node->labelBounds);
    }
    return result;
}
