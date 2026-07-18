#include "TreeSceneItem.h"

#include "DirectoryPresentationPolicy.h"
#include "FakeTreeData.h"
#include "LabelVisibilityPolicy.h"
#include "ThemeTokens.h"

#include <QColor>
#include <QCursor>
#include <QEasingCurve>
#include <QHoverEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
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

    m_focusAnimation.setDuration(ThemeTokens::active().focusAnimationDuration);
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
    const ThemeValues &theme = ThemeTokens::active();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), theme.sceneCanvas);
    painter->save();
    painter->translate(m_pan);
    painter->scale(m_zoom, m_zoom);

    QVector<LayoutNode> labelLayout = m_layout;
    for (LayoutNode &layout : labelLayout) {
        const QPointF position = drawnPosition(layout);
        layout.labelBounds.translate(position - layout.position);
    }
    const QSet<NodeId> visibleLabels = LabelVisibilityPolicy::visibleLabels(
        labelLayout, m_navigation.focusedId(), m_navigation.selectedId(),
        m_hovered, m_zoom);

    for (const LayoutNode &layout : m_layout) {
        const FileNode *entry = node(layout.id);
        if (!entry)
            continue;

        const bool directory = entry->isDirectory();
        const bool selected = layout.id == m_navigation.selectedId();
        const bool hovered = layout.id == m_hovered;
        const bool focused = layout.id == m_navigation.focusedId();
        const QPainterPath branch = drawnBranchPath(layout);
        QColor branchColor = theme.wood;
        if (hovered)
            branchColor = theme.woodHover;
        if (layout.parentContext)
            branchColor.setAlphaF(0.48);

        const qreal branchWidth = directory
            ? (focused ? theme.focusedBranchWidth : theme.directoryBranchWidth)
            : theme.fileStemWidth;
        if (directory && (selected || hovered)) {
            QColor emphasis = selected ? theme.selection : theme.hover;
            if (layout.parentContext)
                emphasis.setAlphaF(0.55);
            painter->setPen(QPen(emphasis,
                                 branchWidth + theme.selectionHaloWidth,
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(branch);
        }

        painter->setPen(QPen(branchColor, branchWidth,
                             Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(branch);
    }

    for (const LayoutNode &layout : m_layout) {
        const FileNode *entry = node(layout.id);
        if (!entry)
            continue;
        const QPointF position = drawnPosition(layout);
        const bool selected = layout.id == m_navigation.selectedId();
        const bool hovered = layout.id == m_hovered;
        const qreal radius = layout.radius + (hovered ? 3.0 : 0.0);

        painter->save();
        if (layout.parentContext)
            painter->setOpacity(0.48);
        if (entry->kind == NodeKind::Directory) {
            painter->setPen(QPen(selected ? theme.selection : theme.woodShadow,
                                 selected ? 2.5 : 1.5));
            painter->setBrush(hovered ? theme.woodHover : theme.wood);
            const qreal junctionRadius = hovered
                ? theme.hoveredJunctionRadius : theme.junctionRadius;
            painter->drawEllipse(position, junctionRadius, junctionRadius);
        } else if (entry->kind == NodeKind::Image) {
            painter->setPen(QPen(selected ? theme.selection : theme.woodShadow,
                                 selected ? 4.0 : 2.0));
            painter->setBrush(kindColor(entry->kind));
            for (int petal = 0; petal < 5; ++petal) {
                const qreal angle = qDegreesToRadians(petal * 72.0 - 90.0);
                const QPointF center = position
                    + QPointF(qCos(angle) * radius * 0.7, qSin(angle) * radius * 0.7);
                painter->drawEllipse(center, radius * 0.55, radius * 0.38);
            }
            painter->setBrush(theme.flowerCenter);
            painter->drawEllipse(position, radius * 0.38, radius * 0.38);
        } else if (entry->kind == NodeKind::Archive) {
            painter->setPen(QPen(selected ? theme.selection : theme.woodShadow,
                                 selected ? 4.0 : 2.0));
            painter->setBrush(kindColor(entry->kind));
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
            painter->setPen(QPen(selected ? theme.selection : theme.woodShadow,
                                 selected ? 4.0 : 2.0));
            painter->setBrush(kindColor(entry->kind));
            painter->drawEllipse(position, radius * 0.72, radius);
        }

        if (visibleLabels.contains(layout.id)) {
            painter->setPen(theme.textDark);
            QFont labelFont = painter->font();
            labelFont.setPixelSize(selected ? 14 : entry->isDirectory() ? 13 : 12);
            labelFont.setWeight(selected || entry->isDirectory()
                                    ? QFont::DemiBold : QFont::Normal);
            painter->setFont(labelFont);
            const QString label = entry->name.size() > 24 && !selected && !hovered
                ? entry->name.left(21) + QStringLiteral("…") : entry->name;
            const QRectF labelBounds = layout.labelBounds.translated(
                position - layout.position);
            painter->drawText(labelBounds,
                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, label);
        }
        painter->restore();
    }

    painter->restore();
}

QString TreeSceneItem::selectedName() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected ? selected->name : QString();
}

QString TreeSceneItem::selectedType() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected ? kindName(selected->kind) : QString();
}

QString TreeSceneItem::selectedPath() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected ? selected->path : QString();
}

QString TreeSceneItem::selectedDetail() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected ? selected->detail : QString();
}

bool TreeSceneItem::selectedIsDirectory() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected && selected->isDirectory();
}

QString TreeSceneItem::selectedDirectoryState() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    if (!selected || !selected->isDirectory())
        return QString();

    switch (selected->directoryState) {
    case DirectoryLoadState::Ready: return QStringLiteral("Ready");
    case DirectoryLoadState::Loading: return QStringLiteral("Still scanning");
    case DirectoryLoadState::Partial: return QStringLiteral("Partial results");
    case DirectoryLoadState::Inaccessible: return QStringLiteral("Inaccessible");
    case DirectoryLoadState::Error: return QStringLiteral("Scan failed");
    }
    return QString();
}

qulonglong TreeSceneItem::selectedTotalChildCount() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected && selected->isDirectory() ? selected->totalChildCount : 0;
}

qulonglong TreeSceneItem::selectedMaterializedChildCount() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected && selected->isDirectory()
        ? static_cast<qulonglong>(selected->children.size()) : 0;
}

bool TreeSceneItem::selectedRequiresContents() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected && DirectoryPresentationPolicy::evaluate(*selected)
        .usesContentsFallback;
}

QString TreeSceneItem::selectedDirectoryStatus() const
{
    const FileNode *selected = node(m_navigation.selectedId());
    return selected && selected->isDirectory()
        ? selected->directoryStatus : QString();
}

QStringList TreeSceneItem::selectedChildNames() const
{
    QStringList names;
    const FileNode *selected = node(m_navigation.selectedId());
    if (!selected || !selected->isDirectory())
        return names;

    const DirectoryPresentation presentation =
        DirectoryPresentationPolicy::evaluate(*selected);
    names.reserve(presentation.visibleChildCount);
    for (qsizetype i = 0; i < presentation.visibleChildCount; ++i) {
        if (const FileNode *child = node(selected->children.at(i)))
            names.append(child->name);
    }
    return names;
}

QString TreeSceneItem::breadcrumb() const
{
    const FileNode *focused = node(m_navigation.focusedId());
    return focused ? focused->path : QString();
}

bool TreeSceneItem::canGoUp() const
{
    const FileNode *focused = node(m_navigation.focusedId());
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
    const NodeId target = m_navigation.takeBackTarget();
    if (target == InvalidNodeId)
        return;
    focusNode(target, false);
}

void TreeSceneItem::goUp()
{
    const FileNode *focused = node(m_navigation.focusedId());
    if (focused && focused->parentId != InvalidNodeId)
        focusNode(focused->parentId, true);
}

void TreeSceneItem::focusSelected()
{
    focusNode(m_navigation.selectedId(), true);
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
        const FileNode *entry = node(it->id);
        if (!entry)
            continue;

        if (entry->isDirectory()) {
            QPainterPathStroker hitStroker;
            hitStroker.setWidth(28.0);
            hitStroker.setCapStyle(Qt::RoundCap);
            hitStroker.setJoinStyle(Qt::RoundJoin);
            const QRectF labelBounds = it->labelBounds.translated(
                drawnPosition(*it) - it->position);
            if (hitStroker.createStroke(drawnBranchPath(*it)).contains(scenePosition)
                || labelBounds.contains(scenePosition)) {
                return it->id;
            }
        } else if (QLineF(scenePosition, drawnPosition(*it)).length()
                   <= it->radius + 9.0) {
            return it->id;
        }
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

QPainterPath TreeSceneItem::drawnBranchPath(const LayoutNode &layout) const
{
    const QPointF end = drawnPosition(layout);
    QPointF start = layout.branchStart;
    if (const LayoutNode *parent = layoutNode(layout.parentId))
        start = drawnPosition(*parent);

    const QPointF control1 = start
        + (layout.branchControl1 - layout.branchStart);
    const QPointF control2 = end
        + (layout.branchControl2 - layout.position);
    QPainterPath result(start);
    result.cubicTo(control1, control2, end);
    return result;
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
    const ThemeValues &theme = ThemeTokens::active();
    switch (kind) {
    case NodeKind::Directory: return theme.wood;
    case NodeKind::Document: return theme.document;
    case NodeKind::Image: return theme.image;
    case NodeKind::Audio: return theme.audio;
    case NodeKind::Code: return theme.code;
    case NodeKind::Archive: return theme.archive;
    }
    return theme.document;
}

void TreeSceneItem::selectNode(NodeId id)
{
    if (!node(id) || !m_navigation.select(id))
        return;
    update();
    emit selectionChanged();
}

void TreeSceneItem::focusNode(NodeId id, bool addToHistory)
{
    const FileNode *target = node(id);
    if (!target || !target->isDirectory()
        || id == m_navigation.focusedId()) {
        return;
    }

    m_focusAnimation.stop();
    m_previousPositions.clear();
    for (const LayoutNode &layout : m_layout)
        m_previousPositions.insert(layout.id, drawnPosition(layout));
    m_navigation.focus(id, addToHistory);
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
    m_layout = m_layoutEngine.layout(m_nodes, m_navigation.focusedId());
    m_hovered = InvalidNodeId;
    update();
}
