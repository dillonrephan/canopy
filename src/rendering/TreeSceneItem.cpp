#include "TreeSceneItem.h"

#include "FakeTreeData.h"

#include <QColor>
#include <QCursor>
#include <QEasingCurve>
#include <QHoverEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

namespace {
constexpr qreal kMinZoom = 0.48;
constexpr qreal kMaxZoom = 2.4;
}

TreeSceneItem::TreeSceneItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_nodes(FakeTreeData::create())
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);

    m_focusAnimation.setDuration(190);
    m_focusAnimation.setStartValue(0.0);
    m_focusAnimation.setEndValue(1.0);
    m_focusAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_focusAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_transitionProgress = value.toReal();
                update();
            });
    connect(&m_focusAnimation, &QVariantAnimation::finished, this, [this] {
        m_transitionProgress = 1.0;
        m_previousPositions.clear();
        update();
    });

    relayout();
}

void TreeSceneItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), QColor("#101713"));
    painter->save();
    painter->translate(m_pan);
    painter->scale(m_zoom, m_zoom);

    for (const LayoutNode &layout : m_layout) {
        const LayoutNode *parent = layoutNode(layout.parentId);
        if (!parent)
            continue;
        const FileNode *entry = node(layout.id);
        if (!entry)
            continue;

        const QPointF position = drawnPosition(layout);
        const QPointF parentPosition = drawnPosition(*parent);
        const QPointF delta = position - parentPosition;
        QPainterPath branch(parentPosition);
        branch.cubicTo(parentPosition + QPointF(delta.x() * 0.35, -18.0),
                       position - QPointF(delta.x() * 0.24, 28.0), position);
        QColor branchColor("#765a3d");
        if (layout.parentContext || parent->parentContext)
            branchColor.setAlphaF(0.48);
        painter->setPen(QPen(branchColor, entry->isDirectory() ? 8.0 : 3.5,
                             Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(branch);
    }

    for (const LayoutNode &layout : m_layout) {
        const FileNode *entry = node(layout.id);
        if (!entry)
            continue;
        const QPointF position = drawnPosition(layout);
        const bool selected = layout.id == m_selected;
        const bool hovered = layout.id == m_hovered;
        const qreal radius = layout.radius + (hovered ? 3.0 : 0.0);

        painter->save();
        if (layout.parentContext)
            painter->setOpacity(0.48);
        painter->setPen(QPen(selected ? QColor("#f2d58f") : QColor("#1f2b23"),
                             selected ? 4.0 : 2.0));
        painter->setBrush(kindColor(entry->kind));

        if (entry->kind == NodeKind::Directory) {
            painter->drawEllipse(position, radius * 1.25, radius);
            painter->drawEllipse(position + QPointF(-radius * 0.65, 3.0),
                                 radius * 0.72, radius * 0.58);
            painter->drawEllipse(position + QPointF(radius * 0.68, 2.0),
                                 radius * 0.72, radius * 0.58);
        } else if (entry->kind == NodeKind::Image) {
            for (int petal = 0; petal < 5; ++petal) {
                const qreal angle = qDegreesToRadians(petal * 72.0 - 90.0);
                const QPointF center = position
                    + QPointF(qCos(angle) * radius * 0.7, qSin(angle) * radius * 0.7);
                painter->drawEllipse(center, radius * 0.55, radius * 0.38);
            }
            painter->setBrush(QColor("#f2d58f"));
            painter->drawEllipse(position, radius * 0.38, radius * 0.38);
        } else if (entry->kind == NodeKind::Archive) {
            QPainterPath pod;
            pod.moveTo(position + QPointF(0.0, -radius));
            pod.cubicTo(position + QPointF(radius, -radius * 0.4),
                        position + QPointF(radius, radius * 0.7),
                        position + QPointF(0.0, radius));
            pod.cubicTo(position + QPointF(-radius, radius * 0.7),
                        position + QPointF(-radius, -radius * 0.4),
                        position + QPointF(0.0, -radius));
            painter->drawPath(pod);
        } else {
            painter->drawEllipse(position, radius * 0.72, radius);
        }

        if (m_zoom >= 0.72 || selected || hovered) {
            painter->setPen(QColor("#e6eadf"));
            QFont labelFont = painter->font();
            labelFont.setPixelSize(selected ? 14 : 12);
            labelFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
            painter->setFont(labelFont);
            const QString label = entry->name.size() > 24 && !selected && !hovered
                ? entry->name.left(21) + QStringLiteral("…") : entry->name;
            painter->drawText(QRectF(position.x() - 82.0, position.y() + radius + 8.0,
                                     164.0, 34.0),
                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, label);
        }
        painter->restore();
    }

    painter->restore();
}

QString TreeSceneItem::selectedName() const
{
    const FileNode *selected = node(m_selected);
    return selected ? selected->name : QString();
}

QString TreeSceneItem::selectedType() const
{
    const FileNode *selected = node(m_selected);
    return selected ? kindName(selected->kind) : QString();
}

QString TreeSceneItem::selectedPath() const
{
    const FileNode *selected = node(m_selected);
    return selected ? selected->path : QString();
}

QString TreeSceneItem::selectedDetail() const
{
    const FileNode *selected = node(m_selected);
    return selected ? selected->detail : QString();
}

QString TreeSceneItem::breadcrumb() const
{
    const FileNode *focused = node(m_focused);
    return focused ? focused->path : QString();
}

bool TreeSceneItem::canGoUp() const
{
    const FileNode *focused = node(m_focused);
    return focused && focused->parentId != InvalidNodeId;
}

void TreeSceneItem::setReducedMotion(bool reducedMotion)
{
    if (m_reducedMotion == reducedMotion)
        return;
    m_reducedMotion = reducedMotion;
    if (reducedMotion && m_focusAnimation.state() == QAbstractAnimation::Running)
        m_focusAnimation.stop();
    m_transitionProgress = 1.0;
    m_previousPositions.clear();
    update();
    emit reducedMotionChanged();
}

void TreeSceneItem::resetView()
{
    m_pan = QPointF(120.0, 35.0);
    m_zoom = 1.0;
    update();
    emit cameraChanged();
}

void TreeSceneItem::goBack()
{
    if (m_history.isEmpty())
        return;
    const NodeId target = m_history.takeLast();
    focusNode(target, false);
}

void TreeSceneItem::goUp()
{
    const FileNode *focused = node(m_focused);
    if (focused && focused->parentId != InvalidNodeId)
        focusNode(focused->parentId, true);
}

void TreeSceneItem::focusSelected()
{
    focusNode(m_selected, true);
}

void TreeSceneItem::hoverMoveEvent(QHoverEvent *event)
{
    const NodeId hovered = nodeAt(event->position());
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    update();
}

void TreeSceneItem::hoverLeaveEvent(QHoverEvent *)
{
    m_hovered = InvalidNodeId;
    update();
}

void TreeSceneItem::mousePressEvent(QMouseEvent *event)
{
    m_lastPointer = event->position();
    const NodeId hit = nodeAt(event->position());
    if (event->button() == Qt::MiddleButton || hit == InvalidNodeId) {
        m_panning = true;
        setCursor(QCursor(Qt::ClosedHandCursor));
    } else {
        selectNode(hit);
    }
    event->accept();
}

void TreeSceneItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    const NodeId hit = nodeAt(event->position());
    if (hit != InvalidNodeId) {
        selectNode(hit);
        focusNode(hit, true);
    }
    event->accept();
}

void TreeSceneItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_panning)
        return;
    m_pan += event->position() - m_lastPointer;
    m_lastPointer = event->position();
    update();
    emit cameraChanged();
}

void TreeSceneItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning) {
        m_panning = false;
        unsetCursor();
    }
    event->accept();
}

void TreeSceneItem::wheelEvent(QWheelEvent *event)
{
    const QPointF anchor = toScene(event->position());
    const qreal factor = qPow(1.0015, event->angleDelta().y());
    m_zoom = qBound(kMinZoom, m_zoom * factor, kMaxZoom);
    m_pan = event->position() - anchor * m_zoom;
    update();
    emit cameraChanged();
    event->accept();
}

QPointF TreeSceneItem::toScene(const QPointF &viewPosition) const
{
    return (viewPosition - m_pan) / m_zoom;
}

NodeId TreeSceneItem::nodeAt(const QPointF &viewPosition) const
{
    const QPointF scenePosition = toScene(viewPosition);
    for (auto it = m_layout.crbegin(); it != m_layout.crend(); ++it) {
        if (QLineF(scenePosition, drawnPosition(*it)).length() <= it->radius + 9.0)
            return it->id;
    }
    return InvalidNodeId;
}

const FileNode *TreeSceneItem::node(NodeId id) const
{
    for (const FileNode &entry : m_nodes) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

const LayoutNode *TreeSceneItem::layoutNode(NodeId id) const
{
    for (const LayoutNode &entry : m_layout) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

QPointF TreeSceneItem::drawnPosition(const LayoutNode &layout) const
{
    const auto previous = m_previousPositions.constFind(layout.id);
    if (previous == m_previousPositions.cend() || m_transitionProgress >= 1.0)
        return layout.position;
    return *previous + (layout.position - *previous) * m_transitionProgress;
}

QString TreeSceneItem::kindName(NodeKind kind) const
{
    switch (kind) {
    case NodeKind::Directory: return QStringLiteral("Directory branch");
    case NodeKind::Document: return QStringLiteral("Document leaf");
    case NodeKind::Image: return QStringLiteral("Image flower");
    case NodeKind::Audio: return QStringLiteral("Audio berry");
    case NodeKind::Code: return QStringLiteral("Code fern");
    case NodeKind::Archive: return QStringLiteral("Archive seed pod");
    }
    return QStringLiteral("Unknown growth");
}

QColor TreeSceneItem::kindColor(NodeKind kind) const
{
    switch (kind) {
    case NodeKind::Directory: return QColor("#477a55");
    case NodeKind::Document: return QColor("#8aaa72");
    case NodeKind::Image: return QColor("#d88e9b");
    case NodeKind::Audio: return QColor("#9272aa");
    case NodeKind::Code: return QColor("#5e9c75");
    case NodeKind::Archive: return QColor("#b88955");
    }
    return QColor("#879083");
}

void TreeSceneItem::selectNode(NodeId id)
{
    if (!node(id) || id == m_selected)
        return;
    m_selected = id;
    update();
    emit selectionChanged();
}

void TreeSceneItem::focusNode(NodeId id, bool addToHistory)
{
    const FileNode *target = node(id);
    if (!target || !target->isDirectory() || id == m_focused)
        return;

    m_focusAnimation.stop();
    m_previousPositions.clear();
    for (const LayoutNode &layout : m_layout)
        m_previousPositions.insert(layout.id, drawnPosition(layout));
    if (addToHistory)
        m_history.append(m_focused);

    m_focused = id;
    m_selected = id;
    relayout();
    resetView();

    if (m_reducedMotion) {
        m_transitionProgress = 1.0;
        m_previousPositions.clear();
    } else {
        m_transitionProgress = 0.0;
        m_focusAnimation.start();
    }

    emit selectionChanged();
    emit navigationChanged();
}

void TreeSceneItem::relayout()
{
    m_layout = m_layoutEngine.layout(m_nodes, m_focused);
    m_hovered = InvalidNodeId;
    update();
}
