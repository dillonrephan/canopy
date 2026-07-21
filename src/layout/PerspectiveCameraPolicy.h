#pragma once

#include "ThemeProfile.h"
#include "WorldLayout.h"

#include <QSizeF>

struct PerspectiveTravelRange {
    qreal minimum = 0.0;
    qreal maximum = 0.0;

    [[nodiscard]] qreal clamp(qreal value) const;
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

    [[nodiscard]] static qreal openSpaceTarget(
        qreal currentDepth,
        qreal viewY,
        const QSizeF &viewport,
        const PerspectiveTravelRange &range,
        const PerspectiveProfile &profile);
};
