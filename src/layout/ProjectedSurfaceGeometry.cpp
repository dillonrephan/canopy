#include "ProjectedSurfaceGeometry.h"

#include <QPainterPathStroker>
#include <QtMath>

namespace {
QPointF interpolatePoint(
    const QPointF &from, const QPointF &to, qreal progress)
{
    return from + (to - from) * progress;
}

qreal interpolateReal(qreal from, qreal to, qreal progress)
{
    return from + (to - from) * progress;
}

QRectF interpolateRect(
    const QRectF &from, const QRectF &to, qreal progress)
{
    return {
        interpolatePoint(from.topLeft(), to.topLeft(), progress),
        QSizeF(
            interpolateReal(from.width(), to.width(), progress),
            interpolateReal(from.height(), to.height(), progress))
    };
}

QPainterPath taperedSurface(const ProjectedNode &node)
{
    const QPointF delta = node.position - node.branchStart;
    const qreal length = qSqrt(
        delta.x() * delta.x() + delta.y() * delta.y());
    if (length <= 0.001) {
        QPainterPath dot;
        const qreal radius = qMax(
            node.branchStartWidth, node.branchEndWidth) * 0.5;
        dot.addEllipse(node.position, radius, radius);
        return dot;
    }

    const QPointF normal(-delta.y() / length, delta.x() / length);
    const qreal startHalf = node.branchStartWidth * 0.5;
    const qreal endHalf = node.branchEndWidth * 0.5;
    const qreal control1Half =
        startHalf + (endHalf - startHalf) * 0.34;
    const qreal control2Half =
        startHalf + (endHalf - startHalf) * 0.72;

    QPainterPath surface;
    surface.moveTo(node.branchStart + normal * startHalf);
    surface.cubicTo(
        node.branchControl1 + normal * control1Half,
        node.branchControl2 + normal * control2Half,
        node.position + normal * endHalf);
    surface.lineTo(node.position - normal * endHalf);
    surface.cubicTo(
        node.branchControl2 - normal * control2Half,
        node.branchControl1 - normal * control1Half,
        node.branchStart - normal * startHalf);
    surface.closeSubpath();
    return surface;
}

QPainterPath paddedHitArea(
    const QPainterPath &surface, qreal padding)
{
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(qreal(0.0), padding) * 2.0);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return surface.united(stroker.createStroke(surface));
}
}

void ProjectedSurfaceGeometry::rebuild(
    ProjectedNode &node, qreal hitPadding)
{
    node.branchSurface = taperedSurface(node);
    node.hitArea = paddedHitArea(node.branchSurface, hitPadding);
}

ProjectedNode ProjectedSurfaceGeometry::interpolate(
    const ProjectedNode &from,
    const ProjectedNode &to,
    qreal progress,
    qreal hitPadding)
{
    if (!qIsFinite(progress))
        progress = 1.0;
    progress = qBound(qreal(0.0), progress, qreal(1.0));
    if (progress <= 0.0)
        return from;
    if (progress >= 1.0)
        return to;

    ProjectedNode result = to;
    result.position = interpolatePoint(
        from.position, to.position, progress);
    result.radius = interpolateReal(
        from.radius, to.radius, progress);
    result.labelBounds = interpolateRect(
        from.labelBounds, to.labelBounds, progress);
    result.branchStart = interpolatePoint(
        from.branchStart, to.branchStart, progress);
    result.branchControl1 = interpolatePoint(
        from.branchControl1, to.branchControl1, progress);
    result.branchControl2 = interpolatePoint(
        from.branchControl2, to.branchControl2, progress);
    result.branchStartWidth = interpolateReal(
        from.branchStartWidth, to.branchStartWidth, progress);
    result.branchEndWidth = interpolateReal(
        from.branchEndWidth, to.branchEndWidth, progress);
    result.crownEnd = interpolatePoint(
        from.crownEnd, to.crownEnd, progress);
    result.depthOpacity = interpolateReal(
        from.depthOpacity, to.depthOpacity, progress);
    result.cameraDepth = interpolateReal(
        from.cameraDepth, to.cameraDepth, progress);
    result.apparentScale = interpolateReal(
        from.apparentScale, to.apparentScale, progress);
    rebuild(result, hitPadding);
    return result;
}
