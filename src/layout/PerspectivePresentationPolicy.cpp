#include "PerspectivePresentationPolicy.h"

#include <algorithm>

bool PerspectiveDirectChildWindow::isForeground(NodeId id) const
{
    return foregroundIds.contains(id);
}

bool PerspectiveDirectChildWindow::isFogHint(NodeId id) const
{
    return fogHintIds.contains(id);
}

PerspectiveDirectChildWindow PerspectivePresentationPolicy::select(
    const WorldLayout &world,
    const ViewState &view,
    const PerspectiveProfile &profile)
{
    QVector<const WorldNodePlacement *> cameraAhead;
    PerspectiveDirectChildWindow result;
    for (const WorldNodePlacement &placement : world.nodes) {
        if (placement.role != WorldNodeRole::DirectChild)
            continue;
        ++result.materializedDirectChildCount;
        const qreal cameraDepth = profile.cameraClearance
            + placement.distanceAlongParent - view.travelDepth;
        if (cameraDepth > 0.0)
            cameraAhead.append(&placement);
    }

    std::sort(
        cameraAhead.begin(), cameraAhead.end(),
        [](const WorldNodePlacement *left,
           const WorldNodePlacement *right) {
            if (left->distanceAlongParent
                != right->distanceAlongParent) {
                return left->distanceAlongParent
                    < right->distanceAlongParent;
            }
            if (left->lane != right->lane)
                return left->lane < right->lane;
            return left->id < right->id;
        });

    const qsizetype foregroundCount = qMin<qsizetype>(
        profile.foregroundCorridorCount, cameraAhead.size());
    const qsizetype visibleCount = qMin<qsizetype>(
        profile.foregroundCorridorCount + profile.fogHintCount,
        cameraAhead.size());
    result.foregroundIds.reserve(foregroundCount);
    result.fogHintIds.reserve(visibleCount - foregroundCount);
    for (qsizetype i = 0; i < visibleCount; ++i) {
        if (i < foregroundCount)
            result.foregroundIds.append(cameraAhead.at(i)->id);
        else
            result.fogHintIds.append(cameraAhead.at(i)->id);
    }
    return result;
}

quint64 PerspectivePresentationPolicy::hiddenDirectChildCount(
    quint64 totalChildCount,
    qsizetype materializedDirectChildCount,
    qsizetype projectedDirectChildCount)
{
    const quint64 knownChildCount = qMax(
        totalChildCount,
        static_cast<quint64>(materializedDirectChildCount));
    const quint64 projectedCount =
        static_cast<quint64>(projectedDirectChildCount);
    return knownChildCount > projectedCount
        ? knownChildCount - projectedCount : 0;
}

bool PerspectivePresentationPolicy::requiresContents(
    const FileNode &directory,
    const PerspectiveProfile &profile)
{
    if (!directory.isDirectory())
        return false;
    const quint64 budget = static_cast<quint64>(
        profile.foregroundCorridorCount + profile.fogHintCount);
    const quint64 knownChildCount = qMax(
        directory.totalChildCount,
        static_cast<quint64>(directory.children.size()));
    return directory.directoryState != DirectoryLoadState::Ready
        || knownChildCount > budget;
}
