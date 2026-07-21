#pragma once

#include "FileNode.h"
#include "ThemeProfile.h"
#include "ViewState.h"
#include "WorldLayout.h"

#include <QVector>

struct PerspectiveDirectChildWindow {
    QVector<NodeId> foregroundIds;
    QVector<NodeId> fogHintIds;
    qsizetype materializedDirectChildCount = 0;

    [[nodiscard]] bool isForeground(NodeId id) const;
    [[nodiscard]] bool isFogHint(NodeId id) const;
};

class PerspectivePresentationPolicy
{
public:
    [[nodiscard]] static PerspectiveDirectChildWindow select(
        const WorldLayout &world,
        const ViewState &view,
        const PerspectiveProfile &profile);

    [[nodiscard]] static quint64 hiddenDirectChildCount(
        quint64 totalChildCount,
        qsizetype materializedDirectChildCount,
        qsizetype projectedDirectChildCount);

    [[nodiscard]] static bool requiresContents(
        const FileNode &directory,
        const PerspectiveProfile &profile);
};
