#pragma once

#include "ThemeProfile.h"
#include "WorldLayout.h"

#include <QSizeF>

struct PerspectiveTravelRange {
    qreal minimum = 0.0;
    qreal maximum = 0.0;

    [[nodiscard]] qreal clamp(qreal value) const;
};

struct PerspectiveTravelTransition {
    qreal currentDepth = 0.0;
    qreal targetDepth = 0.0;
};

class PerspectiveCameraPolicy
{
public:
    [[nodiscard]] static PerspectiveTravelRange travelRange(
        const WorldLayout &world);

    [[nodiscard]] static qreal wheelTarget(
        qreal currentDepth,
        qreal angleDeltaY,
        qreal pixelDeltaY,
        const PerspectiveTravelRange &range,
        const PerspectiveProfile &profile);

    [[nodiscard]] static PerspectiveTravelTransition clampTransition(
        qreal currentDepth,
        qreal targetDepth,
        const PerspectiveTravelRange &range);

    [[nodiscard]] static PerspectiveTravelTransition resetTransition(
        const PerspectiveTravelRange &range);

    [[nodiscard]] static PerspectiveTravelTransition wheelTransition(
        qreal currentDepth,
        qreal pendingTargetDepth,
        bool animationRunning,
        qreal angleDeltaY,
        qreal pixelDeltaY,
        const PerspectiveTravelRange &range,
        const PerspectiveProfile &profile);

    [[nodiscard]] static qreal openSpaceTarget(
        qreal currentDepth,
        qreal viewY,
        const QSizeF &viewport,
        const PerspectiveTravelRange &range,
        const PerspectiveProfile &profile);
};
