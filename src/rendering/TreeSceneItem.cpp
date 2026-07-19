#include "TreeSceneItem.h"

#include "DirectoryPresentationPolicy.h"
#include "LabelVisibilityPolicy.h"
#include "ThemeRegistry.h"

#include <QColor>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEasingCurve>
#include <QHoverEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QWheelEvent>
#include <QUrl>
#include <QtMath>

namespace {
QPainterPath staticBranchPath(const LayoutNode &layout)
{
    QPainterPath result(layout.branchStart);
    result.cubicTo(layout.branchControl1, layout.branchControl2,
                   layout.position);
    return result;
}
}

TreeSceneItem::TreeSceneItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    const ThemeProfile &theme = ThemeRegistry::active();
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);

    m_focusAnimation.setDuration(theme.motion.focusDuration);
    m_focusAnimation.setStartValue(0.0);
    m_focusAnimation.setEndValue(1.0);
    m_focusAnimation.setEasingCurve(theme.motion.focusEasing);
    connect(&m_focusAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_transitionProgress = value.toReal();
                update();
            });
    connect(&m_focusAnimation, &QVariantAnimation::finished, this, [this] {
        m_transitionProgress = 1.0;
        m_previousPositions.clear();
        m_enteringNodes.clear();
        m_departingLayout.clear();
        update();
    });
    m_liveChangeAnimation.setDuration(theme.motion.liveChangeDuration);
    m_liveChangeAnimation.setStartValue(0.0);
    m_liveChangeAnimation.setEndValue(1.0);
    m_liveChangeAnimation.setEasingCurve(theme.motion.liveChangeEasing);
    connect(&m_liveChangeAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_liveChangeProgress = value.toReal();
                update();
            });
    connect(&m_liveChangeAnimation, &QVariantAnimation::finished, this, [this] {
        m_liveChangeProgress = 1.0;
        m_livePreviousPositions.clear();
        m_liveEnteringNodes.clear();
        m_liveDepartingLayout.clear();
        update();
    });
    m_cameraAnimation.setDuration(theme.motion.cameraDuration);
    m_cameraAnimation.setStartValue(0.0);
    m_cameraAnimation.setEndValue(1.0);
    m_cameraAnimation.setEasingCurve(theme.motion.cameraEasing);
    connect(&m_cameraAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                const qreal progress = value.toReal();
                m_pan = m_cameraStartPan
                    + (m_cameraTargetPan - m_cameraStartPan) * progress;
                m_zoom = m_cameraStartZoom
                    + (m_cameraTargetZoom - m_cameraStartZoom) * progress;
                update();
                emit cameraChanged();
            });
    connect(&m_filesystemModel, &FilesystemTreeModel::treeChanged,
            this, [this](const TreeChangeSet &change) {
                if (!change.added.isEmpty() || !change.removed.isEmpty())
                    applyTreeChange(change);
                else
                    relayout();
                emit selectionChanged();
            });
    connect(this, &TreeSceneItem::selectionChanged, this, [this] {
        const FileNode *selected = node(m_navigation.selectedId());
        m_filesystemModel.setContentsDirectory(
            selected && selected->isDirectory()
                ? selected->id : InvalidNodeId);
    });

    relayout();
    m_filesystemModel.setContentsDirectory(m_filesystemModel.rootId());
}

void TreeSceneItem::paint(QPainter *painter)
{
    const ThemeProfile &profile = ThemeRegistry::active();
    const ThemePalette &theme = profile.palette;
    const BranchStyle &branches = profile.branches;
    const LabelStyle &labels = profile.labels;
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
    QSet<NodeId> visibleLabels = LabelVisibilityPolicy::visibleLabels(
        labelLayout, m_navigation.focusedId(), m_navigation.selectedId(),
        m_hovered, m_zoom);
    for (const LayoutNode &layout : std::as_const(m_layout)) {
        if (LabelVisibilityPolicy::depthOpacity(
                layout.depthOpacity, m_zoom)
            < labels.rearVisibilityThreshold
            && layout.id != m_navigation.selectedId()
            && layout.id != m_hovered) {
            visibleLabels.remove(layout.id);
        }
    }

    const auto paintDeparting =
        [this, painter, &theme, &branches](
            const QVector<LayoutNode> &layouts, qreal opacity) {
        if (opacity <= 0.0 || layouts.isEmpty())
            return;
        painter->save();
        painter->setOpacity(opacity);
        for (const LayoutNode &layout : layouts) {
            const FileNode *entry = node(layout.id);
            if (!entry)
                continue;
            QColor branchColor = theme.wood;
            branchColor.setAlphaF(
                branchColor.alphaF() * layout.depthOpacity);
            const qreal depthScale = 1.0 - layout.depthLayer * 0.07;
            const qreal branchWidth = (entry->isDirectory()
                ? branches.directoryWidth : branches.fileStemWidth)
                * depthScale;
            painter->setPen(QPen(branchColor, branchWidth,
                                 Qt::SolidLine, Qt::RoundCap));
            painter->drawPath(staticBranchPath(layout));
            painter->setPen(Qt::NoPen);
            painter->setBrush(branchColor);
            painter->drawEllipse(layout.position,
                                 branches.junctionRadius,
                                 branches.junctionRadius);
        }
        painter->restore();
    };
    paintDeparting(m_departingLayout, 1.0 - m_transitionProgress);
    paintDeparting(m_liveDepartingLayout, 1.0 - m_liveChangeProgress);

    if (const LayoutNode *focusedLayout =
            layoutNode(m_navigation.focusedId());
        focusedLayout && focusedLayout->crownEnd != focusedLayout->position) {
        const QPainterPath crown = drawnCrownPath(*focusedLayout);
        painter->setPen(QPen(theme.wood,
                             branches.focusedWidth * 0.72,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(crown);
    }

    for (const LayoutNode &layout : m_layout) {
        const FileNode *entry = node(layout.id);
        if (!entry)
            continue;

        const qreal focusArrivalOpacity = m_enteringNodes.contains(layout.id)
            ? m_transitionProgress : 1.0;
        const qreal liveArrivalOpacity =
            m_liveEnteringNodes.contains(layout.id)
            ? m_liveChangeProgress : 1.0;
        const qreal arrivalOpacity =
            focusArrivalOpacity * liveArrivalOpacity;
        const qreal depthOpacity = LabelVisibilityPolicy::depthOpacity(
            layout.depthOpacity, m_zoom);
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
        else
            branchColor.setAlphaF(
                branchColor.alphaF() * depthOpacity * arrivalOpacity);

        const qreal depthScale = 1.0 - layout.depthLayer * 0.07;
        const qreal branchWidth = (directory
            ? (focused ? branches.focusedWidth : branches.directoryWidth)
            : branches.fileStemWidth) * depthScale;
        if (directory && (selected || hovered)) {
            QColor emphasis = selected ? theme.selection : theme.hover;
            if (layout.parentContext)
                emphasis.setAlphaF(0.55);
            painter->setPen(QPen(emphasis,
                                 branchWidth + branches.selectionHaloWidth,
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
        const qreal focusArrivalOpacity = m_enteringNodes.contains(layout.id)
            ? m_transitionProgress : 1.0;
        const qreal liveArrivalOpacity =
            m_liveEnteringNodes.contains(layout.id)
            ? m_liveChangeProgress : 1.0;
        const qreal arrivalOpacity =
            focusArrivalOpacity * liveArrivalOpacity;
        const qreal depthOpacity = LabelVisibilityPolicy::depthOpacity(
            layout.depthOpacity, m_zoom);
        const QPointF position = drawnPosition(layout);
        const bool selected = layout.id == m_navigation.selectedId();
        const bool hovered = layout.id == m_hovered;
        const qreal radius = layout.radius + (hovered ? 3.0 : 0.0);

        painter->save();
        painter->setOpacity(
            (layout.parentContext ? 0.48 : depthOpacity)
            * arrivalOpacity);
        if (entry->kind == NodeKind::Directory) {
            painter->setPen(QPen(selected ? theme.selection : theme.woodShadow,
                                 selected ? 2.5 : 1.5));
            painter->setBrush(hovered ? theme.woodHover : theme.wood);
            const qreal junctionRadius = hovered
                ? branches.hoveredJunctionRadius
                : branches.junctionRadius;
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
            labelFont.setPixelSize(
                selected ? labels.selectedPixelSize
                : entry->isDirectory() ? labels.directoryPixelSize
                : labels.filePixelSize);
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

    if (const LayoutNode *focusedLayout =
            layoutNode(m_navigation.focusedId());
        focusedLayout && focusedLayout->hiddenChildCount > 0) {
        painter->save();
        painter->setPen(theme.textDark);
        QFont countFont = painter->font();
        countFont.setPixelSize(labels.overflowPixelSize);
        countFont.setWeight(QFont::DemiBold);
        painter->setFont(countFont);
        const QString countText = QStringLiteral("+%1 more in Contents")
            .arg(focusedLayout->hiddenChildCount);
        const QRectF countBounds(
            focusedLayout->position.x() + 28.0,
            focusedLayout->position.y() + 64.0,
            164.0, 28.0);
        painter->setPen(QPen(theme.border, 1.0));
        painter->setBrush(theme.raisedSurface);
        painter->drawRoundedRect(countBounds, 8.0, 8.0);
        painter->setPen(theme.textDark);
        painter->drawText(countBounds, Qt::AlignCenter, countText);
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
    case DirectoryLoadState::Unloaded: return QStringLiteral("Not loaded");
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
    if (reducedMotion
        && m_liveChangeAnimation.state() == QAbstractAnimation::Running) {
        m_liveChangeAnimation.stop();
    }
    if (reducedMotion
        && m_cameraAnimation.state() == QAbstractAnimation::Running) {
        m_cameraAnimation.stop();
        m_pan = m_cameraTargetPan;
        m_zoom = m_cameraTargetZoom;
    }
    m_transitionProgress = 1.0;
    m_liveChangeProgress = 1.0;
    m_previousPositions.clear();
    m_livePreviousPositions.clear();
    m_enteringNodes.clear();
    m_liveEnteringNodes.clear();
    m_departingLayout.clear();
    m_liveDepartingLayout.clear();
    update();
    emit reducedMotionChanged();
}

void TreeSceneItem::setShowHidden(bool showHidden)
{
    if (m_filesystemModel.includeHidden() == showHidden)
        return;
    m_filesystemModel.setIncludeHidden(showHidden);
    emit showHiddenChanged();
}

void TreeSceneItem::resetView()
{
    m_cameraAnimation.stop();
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

bool TreeSceneItem::navigateToPath(const QString &path)
{
    QString requestedPath = path.trimmed();
    if (requestedPath == QStringLiteral("~")) {
        requestedPath = QDir::homePath();
    } else if (requestedPath.startsWith(QStringLiteral("~/"))) {
        requestedPath = QDir::home().filePath(requestedPath.sliced(2));
    } else if (QDir::isRelativePath(requestedPath)) {
        requestedPath = QDir(breadcrumb()).absoluteFilePath(requestedPath);
    }

    QString errorMessage;
    const NodeId id =
        m_filesystemModel.ensureDirectory(requestedPath, &errorMessage);
    if (id == InvalidNodeId) {
        setPathMessage(errorMessage);
        return false;
    }

    setPathMessage(QString());
    if (id == m_navigation.focusedId()) {
        m_filesystemModel.setActiveDirectory(id);
        m_filesystemModel.setContentsDirectory(id);
        return true;
    }
    focusNode(id, true);
    return true;
}

void TreeSceneItem::activateContentsRow(int row)
{
    DirectoryContentsModel *contents = m_filesystemModel.contentsModel();
    const QString path = contents->pathAt(row);
    if (path.isEmpty())
        return;
    if (contents->isDirectoryAt(row))
        navigateToPath(path);
    else
        openFile(path);
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
    m_cameraAnimation.stop();
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
        const FileNode *entry = node(hit);
        if (entry && entry->isDirectory())
            focusNode(hit, true);
        else if (entry)
            openFile(entry->path);
    } else {
        m_panning = false;
        unsetCursor();
        zoomAt(event->position(),
               ThemeRegistry::active().camera.blankDoubleClickFactor,
               true);
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
    const qreal factor = qPow(
        ThemeRegistry::active().camera.wheelBase,
        event->angleDelta().y());
    zoomAt(event->position(), factor, false);
    event->accept();
}

QPointF TreeSceneItem::toScene(const QPointF &viewPosition) const
{
    return (viewPosition - m_pan) / m_zoom;
}

void TreeSceneItem::zoomAt(
    const QPointF &viewPosition, qreal factor, bool recenter)
{
    m_cameraAnimation.stop();
    const CameraProfile &camera = ThemeRegistry::active().camera;
    const QPointF anchor = toScene(viewPosition);
    const qreal nextZoom = qBound(
        camera.minimumZoom, m_zoom * factor, camera.maximumZoom);
    const QPointF destination = recenter
        ? boundingRect().center() : viewPosition;
    const QPointF nextPan = destination - anchor * nextZoom;
    if (recenter && !m_reducedMotion) {
        m_cameraStartPan = m_pan;
        m_cameraTargetPan = nextPan;
        m_cameraStartZoom = m_zoom;
        m_cameraTargetZoom = nextZoom;
        m_cameraAnimation.start();
        return;
    }
    m_zoom = nextZoom;
    m_pan = nextPan;
    update();
    emit cameraChanged();
}

NodeId TreeSceneItem::nodeAt(const QPointF &viewPosition) const
{
    const QPointF scenePosition = toScene(viewPosition);
    for (auto it = m_layout.crbegin(); it != m_layout.crend(); ++it) {
        const FileNode *entry = node(it->id);
        if (!entry
            || (m_enteringNodes.contains(it->id)
                && m_transitionProgress < 0.35)
            || (m_liveEnteringNodes.contains(it->id)
                && m_liveChangeProgress < 0.35)
            || it->depthOpacity < 0.28) {
            continue;
        }

        if (entry->isDirectory()) {
            QPainterPathStroker hitStroker;
            hitStroker.setWidth(28.0);
            hitStroker.setCapStyle(Qt::RoundCap);
            hitStroker.setJoinStyle(Qt::RoundJoin);
            const QRectF labelBounds = it->labelBounds.translated(
                drawnPosition(*it) - it->position);
            if (hitStroker.createStroke(drawnBranchPath(*it)).contains(scenePosition)
                || (it->id == m_navigation.focusedId()
                    && hitStroker.createStroke(drawnCrownPath(*it))
                        .contains(scenePosition))
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
    return m_filesystemModel.node(id);
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
    QPointF position = layout.position;
    if (previous != m_previousPositions.cend()
        && m_transitionProgress < 1.0) {
        position = *previous
            + (layout.position - *previous) * m_transitionProgress;
    }
    const auto livePrevious = m_livePreviousPositions.constFind(layout.id);
    if (livePrevious != m_livePreviousPositions.cend()
        && m_liveChangeProgress < 1.0) {
        position = *livePrevious
            + (position - *livePrevious) * m_liveChangeProgress;
    }
    return position;
}

QPainterPath TreeSceneItem::drawnBranchPath(const LayoutNode &layout) const
{
    const QPointF end = drawnPosition(layout);
    QPointF start = layout.branchStart;
    if (const LayoutNode *parent = layoutNode(layout.parentId)) {
        if (layout.followsParentPosition) {
            start = drawnPosition(*parent);
        } else {
            start += drawnPosition(*parent) - parent->position;
        }
    }

    const QPointF control1 = start
        + (layout.branchControl1 - layout.branchStart);
    const QPointF control2 = end
        + (layout.branchControl2 - layout.position);
    QPainterPath result(start);
    result.cubicTo(control1, control2, end);
    return result;
}

QPainterPath TreeSceneItem::drawnCrownPath(const LayoutNode &layout) const
{
    const QPointF start = drawnPosition(layout);
    const qreal growth = m_transitionProgress < 1.0
        ? m_transitionProgress : 1.0;
    const QPointF end = start
        + (layout.crownEnd - layout.position) * growth;
    const QPointF delta = end - start;
    QPainterPath result(start);
    result.cubicTo(
        start + delta * 0.34 + QPointF(-3.0, 0.0),
        start + delta * 0.72 + QPointF(4.0, 0.0),
        end);
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
    const ThemePalette &theme = ThemeRegistry::active().palette;
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
    m_departingLayout.clear();
    m_enteringNodes.clear();
    m_previousPositions.clear();
    for (const LayoutNode &layout : m_layout)
        m_previousPositions.insert(layout.id, drawnPosition(layout));
    const QVector<LayoutNode> previousLayout = m_layout;
    m_navigation.focus(id, addToHistory);
    relayout();

    QSet<NodeId> currentIds;
    currentIds.reserve(m_layout.size());
    for (const LayoutNode &layout : std::as_const(m_layout))
        currentIds.insert(layout.id);
    for (const LayoutNode &layout : previousLayout) {
        if (!currentIds.contains(layout.id))
            m_departingLayout.append(layout);
    }
    for (const LayoutNode &layout : std::as_const(m_layout)) {
        if (m_previousPositions.contains(layout.id))
            continue;
        m_previousPositions.insert(layout.id, layout.branchStart);
        m_enteringNodes.insert(layout.id);
    }
    resetView();

    if (m_reducedMotion) {
        m_transitionProgress = 1.0;
        m_previousPositions.clear();
        m_enteringNodes.clear();
        m_departingLayout.clear();
    } else {
        m_transitionProgress = 0.0;
        m_focusAnimation.start();
    }

    emit selectionChanged();
    emit navigationChanged();
    m_filesystemModel.setActiveDirectory(id);
}

void TreeSceneItem::openFile(const QString &path)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        setPathMessage(QString());
    else
        setPathMessage(QStringLiteral("No application could open this file."));
}

void TreeSceneItem::setPathMessage(const QString &message)
{
    if (m_pathMessage == message)
        return;
    m_pathMessage = message;
    emit pathMessageChanged();
}

void TreeSceneItem::applyTreeChange(const TreeChangeSet &change)
{
    m_liveChangeAnimation.stop();
    m_liveChangeProgress = 1.0;
    m_livePreviousPositions.clear();
    m_liveEnteringNodes.clear();
    m_liveDepartingLayout.clear();

    const QVector<LayoutNode> previousLayout = m_layout;
    relayout();

    const QSet<NodeId> removed(
        change.removed.cbegin(), change.removed.cend());
    for (const LayoutNode &layout : previousLayout) {
        if (removed.contains(layout.id))
            m_liveDepartingLayout.append(layout);
    }

    const QSet<NodeId> added(change.added.cbegin(), change.added.cend());
    for (const LayoutNode &layout : std::as_const(m_layout)) {
        if (!added.contains(layout.id))
            continue;
        m_liveEnteringNodes.insert(layout.id);
        m_livePreviousPositions.insert(layout.id, layout.branchStart);
    }

    if (m_reducedMotion
        || (m_liveEnteringNodes.isEmpty()
            && m_liveDepartingLayout.isEmpty())) {
        m_liveDepartingLayout.clear();
        m_livePreviousPositions.clear();
        m_liveEnteringNodes.clear();
        return;
    }

    m_liveChangeProgress = 0.0;
    m_liveChangeAnimation.start();
}

void TreeSceneItem::relayout()
{
    m_layout = m_layoutEngine.layout(
        m_filesystemModel.nodes(), m_navigation.focusedId());
    m_hovered = InvalidNodeId;
    update();
}
