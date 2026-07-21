#include "PerspectiveCameraPolicy.h"

#include <QtMath>

#include <limits>

namespace {
qreal wheelSteps(
    qreal angleDeltaY,
    qreal pixelDeltaY,
    const PerspectiveProfile &profile)
{
    if (angleDeltaY != 0.0 && !qIsNaN(angleDeltaY))
        return angleDeltaY / 120.0;
    if (pixelDeltaY != 0.0
        && !qIsNaN(pixelDeltaY)
        && qIsFinite(profile.touchpadPixelsPerStep)
        && profile.touchpadPixelsPerStep > 0.0) {
        return pixelDeltaY / profile.touchpadPixelsPerStep;
    }
    return 0.0;
}
}

qreal PerspectiveTravelRange::clamp(qreal value) const
{
    const qreal safeMinimum = qIsFinite(minimum)
        ? qMax(0.0, minimum) : 0.0;
    const qreal safeMaximum = qIsFinite(maximum)
        ? qMax(safeMinimum, maximum) : safeMinimum;
    if (qIsNaN(value))
        return safeMinimum;
    if (value == std::numeric_limits<qreal>::infinity())
        return safeMaximum;
    if (value == -std::numeric_limits<qreal>::infinity())
        return safeMinimum;
    return qBound(safeMinimum, value, safeMaximum);
}

PerspectiveTravelRange PerspectiveCameraPolicy::travelRange(
    const WorldLayout &world)
{
    qreal maximum = 0.0;
    for (const WorldNodePlacement &placement : world.nodes) {
        if (placement.role != WorldNodeRole::DirectChild
            || !qIsFinite(placement.distanceAlongParent)
            || placement.distanceAlongParent <= 0.0) {
            continue;
        }
        maximum = qMax(maximum, placement.distanceAlongParent);
    }
    return {.minimum = 0.0, .maximum = maximum};
}

qreal PerspectiveCameraPolicy::wheelTarget(
    qreal currentDepth,
    qreal angleDeltaY,
    qreal pixelDeltaY,
    const PerspectiveTravelRange &range,
    const PerspectiveProfile &profile)
{
    const qreal current = range.clamp(currentDepth);
    const qreal steps = wheelSteps(
        angleDeltaY, pixelDeltaY, profile);

    if (qIsNaN(steps)
        || !qIsFinite(profile.wheelTravelPerStep)
        || profile.wheelTravelPerStep <= 0.0) {
        return current;
    }
    return range.clamp(
        current + steps * profile.wheelTravelPerStep);
}

PerspectiveTravelTransition PerspectiveCameraPolicy::clampTransition(
    qreal currentDepth,
    qreal targetDepth,
    const PerspectiveTravelRange &range)
{
    return {
        .currentDepth = range.clamp(currentDepth),
        .targetDepth = range.clamp(targetDepth),
    };
}

PerspectiveTravelTransition PerspectiveCameraPolicy::resetTransition(
    const PerspectiveTravelRange &range)
{
    const qreal baseline = range.clamp(range.minimum);
    return {
        .currentDepth = baseline,
        .targetDepth = baseline,
    };
}

PerspectiveTravelTransition PerspectiveCameraPolicy::wheelTransition(
    qreal currentDepth,
    qreal pendingTargetDepth,
    bool animationRunning,
    qreal angleDeltaY,
    qreal pixelDeltaY,
    const PerspectiveTravelRange &range,
    const PerspectiveProfile &profile)
{
    const PerspectiveTravelTransition clamped = clampTransition(
        currentDepth, pendingTargetDepth, range);
    const qreal steps = wheelSteps(
        angleDeltaY, pixelDeltaY, profile);
    if (qFuzzyIsNull(steps) || qIsNaN(steps))
        return clamped;

    qreal base = clamped.currentDepth;
    const qreal pendingDelta =
        clamped.targetDepth - clamped.currentDepth;
    if (animationRunning
        && !qFuzzyIsNull(pendingDelta)
        && ((pendingDelta > 0.0) == (steps > 0.0))) {
        base = clamped.targetDepth;
    }

    return {
        .currentDepth = clamped.currentDepth,
        .targetDepth = wheelTarget(
            base,
            angleDeltaY,
            pixelDeltaY,
            range,
            profile),
    };
}

qreal PerspectiveCameraPolicy::openSpaceTarget(
    qreal currentDepth,
    qreal viewY,
    const QSizeF &viewport,
    const PerspectiveTravelRange &range,
    const PerspectiveProfile &profile)
{
    const qreal current = range.clamp(currentDepth);
    if (!qIsFinite(viewY)
        || !qIsFinite(viewport.height())
        || viewport.height() <= 0.0
        || !qIsFinite(profile.horizonYRatio)
        || !qIsFinite(profile.nearYRatio)
        || !qIsFinite(profile.focalDepth)
        || profile.focalDepth <= 0.0
        || !qIsFinite(profile.cameraClearance)
        || profile.cameraClearance <= 0.0) {
        return current;
    }

    const qreal horizonY =
        viewport.height() * profile.horizonYRatio;
    const qreal nearY = viewport.height() * profile.nearYRatio;
    const qreal span = nearY - horizonY;
    if (!qIsFinite(horizonY) || !qIsFinite(nearY)
        || !qIsFinite(span) || span <= 0.0) {
        return current;
    }
    if (viewY <= horizonY)
        return range.clamp(range.maximum);

    const qreal scale = (viewY - horizonY) / span;
    if (scale == std::numeric_limits<qreal>::infinity()) {
        return range.clamp(
            current - profile.focalDepth
                - profile.cameraClearance);
    }
    if (!qIsFinite(scale) || scale <= 0.0)
        return range.clamp(range.maximum);
    const qreal cameraDepth =
        profile.focalDepth * (1.0 / scale - 1.0);
    if (!qIsFinite(cameraDepth))
        return range.clamp(cameraDepth);
    const qreal target = current + cameraDepth
        - profile.cameraClearance;
    return range.clamp(target);
}
