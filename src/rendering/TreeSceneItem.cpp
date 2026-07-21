#include "TreeSceneItem.h"

#include "DirectoryPresentationPolicy.h"
#include "LabelVisibilityPolicy.h"
#include "PerspectivePresentationPolicy.h"
#include "ProjectedSurfaceGeometry.h"
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

#include <algorithm>

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
    : TreeSceneItem(QDir::homePath(), parent)
{
}

TreeSceneItem::TreeSceneItem(
    const QString &rootPath, QQuickItem *parent)
    : TreeSceneItem(
        rootPath,
        ThemeRegistry::active().projection,
        parent)
{
}

TreeSceneItem::TreeSceneItem(
    const QString &rootPath,
    ProjectionKind projectionKind,
    QQuickItem *parent)
    : QQuickPaintedItem(parent),
      m_filesystemModel(rootPath),
      m_layoutEngine(projectionKind)
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
        m_previousProjectedLayout.clear();
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
        m_livePreviousProjectedLayout.clear();
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
    m_travelAnimation.setDuration(theme.motion.cameraDuration);
    m_travelAnimation.setStartValue(0.0);
    m_travelAnimation.setEndValue(1.0);
    m_travelAnimation.setEasingCurve(theme.motion.cameraEasing);
    connect(&m_travelAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                const qreal progress = value.toReal();
                m_travelDepth = perspectiveTravelRange().clamp(
                    m_travelStartDepth
                    + (m_travelTargetDepth - m_travelStartDepth)
                        * progress);
                relayout(false);
                emit cameraChanged();
            });
    connect(&m_travelAnimation, &QVariantAnimation::finished, this, [this] {
        m_travelDepth = perspectiveTravelRange().clamp(
            m_travelTargetDepth);
        m_travelStartDepth = m_travelDepth;
        m_travelTargetDepth = m_travelDepth;
    });
    connect(&m_filesystemModel, &FilesystemTreeModel::treeChanged,
            this, [this](const TreeChangeSet &change) {
                handleTreeChange(change);
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

void TreeSceneItem::geometryChange(
    const QRectF &newGeometry,
    const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (usesPerspectiveProjection()
        && newGeometry.size() != oldGeometry.size()) {
        relayout();
    }
}

void TreeSceneItem::paint(QPainter *painter)
{
    const ThemeProfile &profile = ThemeRegistry::active();
    const bool perspective = usesPerspectiveProjection();
    const qreal presentationZoom = perspective ? 1.0 : m_zoom;
    const ThemePalette &theme = profile.palette;
    const BranchStyle &branches = profile.branches;
    const LabelStyle &labels = profile.labels;
    const QVector<LayoutNode> renderedLayout = perspective
        ? resolvedPerspectiveLayout() : m_layout;
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), theme.sceneCanvas);
    painter->save();
    if (!perspective) {
        painter->translate(m_pan);
        painter->scale(m_zoom, m_zoom);
    }

    const QSet<NodeId> visibleLabels =
        visibleLabelIds(presentationZoom, renderedLayout);

    const auto paintDeparting =
        [this, painter, &theme, &branches, perspective](
            const QVector<DepartureSnapshot> &snapshots, qreal opacity) {
        if (opacity <= 0.0 || snapshots.isEmpty())
            return;
        painter->save();
        painter->setOpacity(opacity);
        for (const DepartureSnapshot &snapshot : snapshots) {
            const LayoutNode &layout = snapshot.layout;
            QColor branchColor = theme.wood;
            branchColor.setAlphaF(
                branchColor.alphaF() * layout.depthOpacity);
            if (perspective) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(branchColor);
                painter->drawPath(layout.branchSurface);
                painter->drawEllipse(layout.position,
                                     branches.junctionRadius,
                                     branches.junctionRadius);
                continue;
            }
            const qreal depthScale = 1.0 - layout.depthLayer * 0.07;
            const qreal branchWidth = (snapshot.kind == NodeKind::Directory
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
        !perspective && focusedLayout
        && focusedLayout->crownEnd != focusedLayout->position) {
        const QPainterPath crown = drawnCrownPath(*focusedLayout);
        painter->setPen(QPen(theme.wood,
                             branches.focusedWidth * 0.72,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(crown);
    }

    for (const LayoutNode &layout : renderedLayout) {
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
            layout.depthOpacity, presentationZoom);
        const bool directory = entry->isDirectory();
        const bool selected = layout.id == m_navigation.selectedId();
        const bool hovered = layout.id == m_hovered;
        const bool focused = layout.id == m_navigation.focusedId();
        QColor branchColor = theme.wood;
        if (hovered)
            branchColor = theme.woodHover;
        if (layout.parentContext)
            branchColor.setAlphaF(0.48);
        else
            branchColor.setAlphaF(
                branchColor.alphaF() * depthOpacity * arrivalOpacity);

        if (perspective) {
            if (directory && (selected || hovered)) {
                QColor emphasis = selected ? theme.selection : theme.hover;
                if (layout.parentContext)
                    emphasis.setAlphaF(0.55);
                painter->setPen(QPen(
                    emphasis, branches.selectionHaloWidth,
                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            } else {
                painter->setPen(Qt::NoPen);
            }
            painter->setBrush(branchColor);
            painter->drawPath(layout.branchSurface);
            continue;
        }

        const QPainterPath branch = drawnBranchPath(layout);
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

    for (const LayoutNode &layout : renderedLayout) {
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
            layout.depthOpacity, presentationZoom);
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

    const auto focusedLayout = std::find_if(
        renderedLayout.cbegin(), renderedLayout.cend(),
        [this](const LayoutNode &layout) {
            return layout.id == m_navigation.focusedId();
        });
    if (focusedLayout != renderedLayout.cend()
        && focusedLayout->hiddenChildCount > 0) {
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
    if (!selected)
        return false;
    if (usesPerspectiveProjection()) {
        return PerspectivePresentationPolicy::requiresContents(
            *selected, ThemeRegistry::active().perspective);
    }
    return DirectoryPresentationPolicy::evaluate(*selected)
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

bool TreeSceneItem::canTravelForward() const noexcept
{
    return usesPerspectiveProjection()
        && m_travelDepth < m_maximumTravelDepth
        && !qFuzzyCompare(
            1.0 + m_travelDepth,
            1.0 + m_maximumTravelDepth);
}

bool TreeSceneItem::canTravelBackward() const noexcept
{
    return usesPerspectiveProjection()
        && m_travelDepth > 0.0
        && !qFuzzyIsNull(m_travelDepth);
}

bool TreeSceneItem::canResetView() const noexcept
{
    if (usesPerspectiveProjection())
        return canTravelBackward();
    return m_pan != QPointF(120.0, 35.0)
        || !qFuzzyCompare(m_zoom, 1.0);
}

void TreeSceneItem::setReducedMotion(bool reducedMotion)
{
    if (m_reducedMotion == reducedMotion)
        return;
    m_reducedMotion = reducedMotion;
    bool planarCameraSnapped = false;
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
        planarCameraSnapped = true;
    }
    if (reducedMotion
        && m_travelAnimation.state() == QAbstractAnimation::Running) {
        const qreal pendingTarget = perspectiveTravelRange().clamp(
            m_travelTargetDepth);
        m_travelAnimation.stop();
        m_travelDepth = pendingTarget;
        m_travelStartDepth = pendingTarget;
        m_travelTargetDepth = pendingTarget;
        relayout(false);
        emit cameraChanged();
    }
    m_transitionProgress = 1.0;
    m_liveChangeProgress = 1.0;
    m_previousPositions.clear();
    m_livePreviousPositions.clear();
    m_previousProjectedLayout.clear();
    m_livePreviousProjectedLayout.clear();
    m_enteringNodes.clear();
    m_liveEnteringNodes.clear();
    m_departingLayout.clear();
    m_liveDepartingLayout.clear();
    update();
    if (planarCameraSnapped)
        emit cameraChanged();
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
    if (usesPerspectiveProjection()) {
        startPerspectiveTravel(0.0);
        return;
    }
    m_cameraAnimation.stop();
    m_pan = QPointF(120.0, 35.0);
    m_zoom = 1.0;
    update();
    emit cameraChanged();
}

void TreeSceneItem::travelForward()
{
    if (usesPerspectiveProjection())
        applyPerspectiveWheel(120.0, 0.0);
}

void TreeSceneItem::travelBackward()
{
    if (usesPerspectiveProjection())
        applyPerspectiveWheel(-120.0, 0.0);
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
    forceActiveFocus(Qt::MouseFocusReason);
    m_cameraAnimation.stop();
    if (usesPerspectiveProjection())
        freezePerspectiveTravel();
    m_lastPointer = event->position();
    const NodeId hit = nodeAt(event->position());
    if (usesPerspectiveProjection()
        && (event->button() == Qt::MiddleButton
            || hit == InvalidNodeId)) {
        event->accept();
        return;
    }
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
        if (usesPerspectiveProjection()) {
            const qreal targetDepth =
                PerspectiveCameraPolicy::openSpaceTarget(
                    m_travelDepth,
                    event->position().y(),
                    {width(), height()},
                    perspectiveTravelRange(),
                    ThemeRegistry::active().perspective);
            startPerspectiveTravel(targetDepth);
            event->accept();
            return;
        }
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
    if (usesPerspectiveProjection())
        return;
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
    if (usesPerspectiveProjection()) {
        applyPerspectiveWheel(
            event->angleDelta().y(), event->pixelDelta().y());
        event->accept();
        return;
    }
    const qreal factor = qPow(
        ThemeRegistry::active().camera.wheelBase,
        event->angleDelta().y());
    zoomAt(event->position(), factor, false);
    event->accept();
}

QPointF TreeSceneItem::toScene(const QPointF &viewPosition) const
{
    if (usesPerspectiveProjection())
        return viewPosition;
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

PerspectiveTravelRange TreeSceneItem::perspectiveTravelRange() const
{
    return {
        .minimum = 0.0,
        .maximum = m_maximumTravelDepth,
    };
}

void TreeSceneItem::finishTransientAnimationsForTravel()
{
    m_focusAnimation.stop();
    m_liveChangeAnimation.stop();
    m_transitionProgress = 1.0;
    m_liveChangeProgress = 1.0;
    m_previousPositions.clear();
    m_livePreviousPositions.clear();
    m_previousProjectedLayout.clear();
    m_livePreviousProjectedLayout.clear();
    m_enteringNodes.clear();
    m_liveEnteringNodes.clear();
    m_departingLayout.clear();
    m_liveDepartingLayout.clear();
    update();
}

void TreeSceneItem::freezePerspectiveTravel()
{
    if (m_travelAnimation.state() != QAbstractAnimation::Running)
        return;
    m_travelAnimation.stop();
    m_travelDepth = perspectiveTravelRange().clamp(m_travelDepth);
    m_travelStartDepth = m_travelDepth;
    m_travelTargetDepth = m_travelDepth;
}

void TreeSceneItem::resetPerspectiveTravel()
{
    m_travelAnimation.stop();
    const PerspectiveTravelTransition reset =
        PerspectiveCameraPolicy::resetTransition(
            perspectiveTravelRange());
    m_travelDepth = reset.currentDepth;
    m_travelStartDepth = reset.currentDepth;
    m_travelTargetDepth = reset.targetDepth;
}

void TreeSceneItem::startPerspectiveTravel(qreal targetDepth)
{
    const qreal target = perspectiveTravelRange().clamp(targetDepth);
    if (m_travelAnimation.state() == QAbstractAnimation::Running
        && qFuzzyCompare(1.0 + target, 1.0 + m_travelTargetDepth)) {
        return;
    }
    if (m_travelAnimation.state() != QAbstractAnimation::Running
        && qFuzzyCompare(1.0 + target, 1.0 + m_travelDepth)) {
        return;
    }

    m_travelAnimation.stop();
    m_travelDepth = perspectiveTravelRange().clamp(m_travelDepth);
    m_travelStartDepth = m_travelDepth;
    m_travelTargetDepth = m_travelDepth;
    if (qFuzzyCompare(1.0 + target, 1.0 + m_travelDepth))
        return;

    finishTransientAnimationsForTravel();
    m_travelTargetDepth = target;
    if (m_reducedMotion) {
        m_travelDepth = target;
        m_travelStartDepth = target;
        m_travelTargetDepth = target;
        relayout(false);
        emit cameraChanged();
        return;
    }
    m_travelAnimation.start();
}

void TreeSceneItem::applyPerspectiveWheel(
    qreal angleDeltaY, qreal pixelDeltaY)
{
    const bool running =
        m_travelAnimation.state() == QAbstractAnimation::Running;
    const PerspectiveTravelTransition transition =
        PerspectiveCameraPolicy::wheelTransition(
            m_travelDepth,
            m_travelTargetDepth,
            running,
            angleDeltaY,
            pixelDeltaY,
            perspectiveTravelRange(),
            ThemeRegistry::active().perspective);
    if (running
        && qFuzzyCompare(
            1.0 + transition.targetDepth,
            1.0 + m_travelTargetDepth)) {
        return;
    }
    if (!running
        && qFuzzyCompare(
            1.0 + transition.targetDepth,
            1.0 + m_travelDepth)) {
        return;
    }
    startPerspectiveTravel(transition.targetDepth);
}

NodeId TreeSceneItem::nodeAt(const QPointF &viewPosition) const
{
    const QPointF scenePosition = toScene(viewPosition);
    const QVector<LayoutNode> renderedLayout = usesPerspectiveProjection()
        ? resolvedPerspectiveLayout() : m_layout;
    const qreal minimumHitOpacity = usesPerspectiveProjection()
        ? ThemeRegistry::active().perspective.minimumHitOpacity
        : 0.28;
    const qreal arrivalHitThreshold = usesPerspectiveProjection()
        ? 1.0 : 0.35;
    const auto isHittable = [this, minimumHitOpacity, arrivalHitThreshold](
                                const LayoutNode &layout) {
        return node(layout.id)
            && !(m_enteringNodes.contains(layout.id)
                 && m_transitionProgress < arrivalHitThreshold)
            && !(m_liveEnteringNodes.contains(layout.id)
                 && m_liveChangeProgress < arrivalHitThreshold)
            && layout.depthOpacity >= minimumHitOpacity;
    };

    if (usesPerspectiveProjection()) {
        const QSet<NodeId> visibleLabels =
            visibleLabelIds(1.0, renderedLayout);

        // Endpoints and labels are painted in a second pass above every branch,
        // so they must receive the first hit pass regardless of branch depth.
        for (auto it = renderedLayout.crbegin();
             it != renderedLayout.crend(); ++it) {
            if (!isHittable(*it))
                continue;
            if ((visibleLabels.contains(it->id)
                 && it->labelBounds.contains(scenePosition))
                || QLineF(scenePosition, it->position).length()
                    <= it->radius + 9.0) {
                return it->id;
            }
        }

        for (auto it = renderedLayout.crbegin();
             it != renderedLayout.crend(); ++it) {
            if (isHittable(*it) && it->hitArea.contains(scenePosition))
                return it->id;
        }
        return InvalidNodeId;
    }

    for (auto it = renderedLayout.crbegin();
         it != renderedLayout.crend(); ++it) {
        const FileNode *entry = node(it->id);
        if (!entry || !isHittable(*it)) {
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

QSet<NodeId> TreeSceneItem::visibleLabelIds(
    qreal presentationZoom,
    const QVector<LayoutNode> &layout) const
{
    const LabelStyle &labels = ThemeRegistry::active().labels;
    QVector<LayoutNode> labelLayout = layout;
    for (LayoutNode &layout : labelLayout) {
        const QPointF position = drawnPosition(layout);
        layout.labelBounds.translate(position - layout.position);
    }
    QSet<NodeId> visibleLabels = LabelVisibilityPolicy::visibleLabels(
        labelLayout, m_navigation.focusedId(), m_navigation.selectedId(),
        m_hovered, presentationZoom);
    for (const LayoutNode &layout : std::as_const(labelLayout)) {
        if (LabelVisibilityPolicy::depthOpacity(
                layout.depthOpacity, presentationZoom)
            < labels.rearVisibilityThreshold
            && layout.id != m_navigation.selectedId()
            && layout.id != m_hovered) {
            visibleLabels.remove(layout.id);
        }
    }
    return visibleLabels;
}

QVector<LayoutNode> TreeSceneItem::resolvedPerspectiveLayout() const
{
    QVector<LayoutNode> result = m_layout;
    const qreal hitPadding =
        ThemeRegistry::active().perspective.hitPadding;
    for (LayoutNode &layout : result) {
        const auto previous = m_previousProjectedLayout.constFind(layout.id);
        if (previous != m_previousProjectedLayout.cend()
            && m_transitionProgress < 1.0) {
            layout = ProjectedSurfaceGeometry::interpolate(
                *previous, layout, m_transitionProgress, hitPadding);
        }
        const auto livePrevious =
            m_livePreviousProjectedLayout.constFind(layout.id);
        if (livePrevious != m_livePreviousProjectedLayout.cend()
            && m_liveChangeProgress < 1.0) {
            layout = ProjectedSurfaceGeometry::interpolate(
                *livePrevious, layout,
                m_liveChangeProgress, hitPadding);
        }
    }
    std::stable_sort(
        result.begin(), result.end(),
        [](const LayoutNode &left, const LayoutNode &right) {
            return left.cameraDepth > right.cameraDepth;
        });
    return result;
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
    // Perspective callers resolve the complete surface before reaching this
    // legacy planar helper.
    if (usesPerspectiveProjection())
        return layout.position;

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

    const bool perspective = usesPerspectiveProjection();
    const QVector<LayoutNode> previousLayout = perspective
        ? resolvedPerspectiveLayout() : m_layout;
    const qreal previousTravelDepth = m_travelDepth;
    const qreal previousMaximumTravelDepth = m_maximumTravelDepth;
    if (perspective) {
        freezePerspectiveTravel();
        finishTransientAnimationsForTravel();
    } else {
        m_focusAnimation.stop();
    }
    m_departingLayout.clear();
    m_enteringNodes.clear();
    m_previousPositions.clear();
    m_previousProjectedLayout.clear();
    for (const LayoutNode &layout : previousLayout)
        m_previousPositions.insert(layout.id, drawnPosition(layout));
    m_navigation.focus(id, addToHistory);
    if (perspective)
        resetPerspectiveTravel();
    relayout(false);
    if (perspective
        && (!qFuzzyCompare(
                1.0 + previousTravelDepth,
                1.0 + m_travelDepth)
            || !qFuzzyCompare(
                1.0 + previousMaximumTravelDepth,
                1.0 + m_maximumTravelDepth))) {
        emit cameraChanged();
    }

    QSet<NodeId> currentIds;
    currentIds.reserve(m_layout.size());
    for (const LayoutNode &layout : std::as_const(m_layout))
        currentIds.insert(layout.id);
    for (const LayoutNode &layout : previousLayout) {
        if (currentIds.contains(layout.id))
            continue;
        const FileNode *entry = node(layout.id);
        if (entry) {
            m_departingLayout.append({
                .layout = layout,
                .kind = entry->kind,
            });
        }
    }
    for (const LayoutNode &layout : std::as_const(m_layout)) {
        const auto previous = std::find_if(
            previousLayout.cbegin(), previousLayout.cend(),
            [&layout](const LayoutNode &entry) {
                return entry.id == layout.id;
            });
        if (previous != previousLayout.cend()) {
            if (perspective)
                m_previousProjectedLayout.insert(layout.id, *previous);
            continue;
        }
        m_previousPositions.insert(layout.id, layout.branchStart);
        m_enteringNodes.insert(layout.id);
    }
    if (!perspective)
        resetView();

    if (m_reducedMotion) {
        m_transitionProgress = 1.0;
        m_previousPositions.clear();
        m_previousProjectedLayout.clear();
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

void TreeSceneItem::handleTreeChange(const TreeChangeSet &change)
{
    QSet<NodeId> invalidNavigationIds;
    invalidNavigationIds.reserve(change.removed.size() + 2);
    for (const RemovedTreeNode &entry : change.removed)
        invalidNavigationIds.insert(entry.id);

    const FileNode *focused = node(m_navigation.focusedId());
    if (!focused || !focused->isDirectory())
        invalidNavigationIds.insert(m_navigation.focusedId());
    if (!node(m_navigation.selectedId()))
        invalidNavigationIds.insert(m_navigation.selectedId());

    const FileNode *changedDirectory = node(change.directoryId);
    const NodeId fallbackId =
        changedDirectory && changedDirectory->isDirectory()
        ? changedDirectory->id : m_filesystemModel.rootId();
    const qreal previousTravelDepth = m_travelDepth;
    const qreal previousMaximumTravelDepth = m_maximumTravelDepth;
    const QPointF previousPan = m_pan;
    const qreal previousZoom = m_zoom;
    const NavigationReconciliation navigation =
        m_navigation.reconcileRemoved(
            invalidNavigationIds, fallbackId);

    if (navigation.focusChanged) {
        if (usesPerspectiveProjection())
            freezePerspectiveTravel();
        finishTransientAnimationsForTravel();
        if (usesPerspectiveProjection()) {
            resetPerspectiveTravel();
        } else {
            m_cameraAnimation.stop();
            m_pan = QPointF(120.0, 35.0);
            m_cameraStartPan = m_pan;
            m_cameraTargetPan = m_pan;
            m_zoom = 1.0;
            m_cameraStartZoom = m_zoom;
            m_cameraTargetZoom = m_zoom;
        }
    }

    if (!change.added.isEmpty() || !change.removed.isEmpty()) {
        applyTreeChange(change, !navigation.focusChanged);
    } else {
        relayout(!navigation.focusChanged);
    }

    if (navigation.focusChanged) {
        const bool cameraChangedForFallback = usesPerspectiveProjection()
            ? !qFuzzyCompare(
                1.0 + previousTravelDepth,
                1.0 + m_travelDepth)
                || !qFuzzyCompare(
                    1.0 + previousMaximumTravelDepth,
                    1.0 + m_maximumTravelDepth)
            : previousPan != m_pan
                || !qFuzzyCompare(previousZoom, m_zoom);
        if (cameraChangedForFallback)
            emit cameraChanged();
    }
    if (navigation.focusChanged || navigation.historyChanged)
        emit navigationChanged();
    emit selectionChanged();
    if (navigation.focusChanged)
        m_filesystemModel.setActiveDirectory(m_navigation.focusedId());
}

void TreeSceneItem::applyTreeChange(
    const TreeChangeSet &change,
    bool notifyPerspectiveCameraChange)
{
    const bool perspective = usesPerspectiveProjection();
    const QVector<LayoutNode> previousLayout = perspective
        ? resolvedPerspectiveLayout() : m_layout;
    m_liveChangeAnimation.stop();
    m_liveChangeProgress = 1.0;
    m_livePreviousPositions.clear();
    m_livePreviousProjectedLayout.clear();
    m_liveEnteringNodes.clear();
    m_liveDepartingLayout.clear();

    if (perspective)
        freezePerspectiveTravel();
    relayout(notifyPerspectiveCameraChange);

    QHash<NodeId, NodeKind> removed;
    removed.reserve(change.removed.size());
    for (const RemovedTreeNode &entry : change.removed)
        removed.insert(entry.id, entry.kind);
    for (const LayoutNode &layout : previousLayout) {
        const auto kind = removed.constFind(layout.id);
        if (kind != removed.cend()) {
            m_liveDepartingLayout.append({
                .layout = layout,
                .kind = *kind,
            });
        }
    }

    const QSet<NodeId> added(change.added.cbegin(), change.added.cend());
    for (const LayoutNode &layout : std::as_const(m_layout)) {
        if (perspective) {
            const auto previous = std::find_if(
                previousLayout.cbegin(), previousLayout.cend(),
                [&layout](const LayoutNode &entry) {
                    return entry.id == layout.id;
                });
            if (previous != previousLayout.cend())
                m_livePreviousProjectedLayout.insert(layout.id, *previous);
        }
        if (!added.contains(layout.id))
            continue;
        m_liveEnteringNodes.insert(layout.id);
        m_livePreviousPositions.insert(layout.id, layout.branchStart);
    }

    if (m_reducedMotion
        || (m_liveEnteringNodes.isEmpty()
            && m_liveDepartingLayout.isEmpty()
            && m_livePreviousProjectedLayout.isEmpty())) {
        m_liveDepartingLayout.clear();
        m_livePreviousPositions.clear();
        m_livePreviousProjectedLayout.clear();
        m_liveEnteringNodes.clear();
        return;
    }

    m_liveChangeProgress = 0.0;
    m_liveChangeAnimation.start();
}

void TreeSceneItem::relayout(bool notifyPerspectiveCameraChange)
{
    bool perspectiveCameraChanged = false;
    if (usesPerspectiveProjection()) {
        const qreal previousMaximum = m_maximumTravelDepth;
        m_maximumTravelDepth = m_layoutEngine.perspectiveTravelRange(
            m_filesystemModel.nodes(),
            m_navigation.focusedId()).maximum;
        const PerspectiveTravelTransition clamped =
            PerspectiveCameraPolicy::clampTransition(
                m_travelDepth,
                m_travelTargetDepth,
                perspectiveTravelRange());
        const qreal clampedStart = perspectiveTravelRange().clamp(
            m_travelStartDepth);
        perspectiveCameraChanged =
            !qFuzzyCompare(1.0 + previousMaximum,
                           1.0 + m_maximumTravelDepth)
            || !qFuzzyCompare(1.0 + m_travelDepth,
                              1.0 + clamped.currentDepth)
            || !qFuzzyCompare(1.0 + m_travelTargetDepth,
                              1.0 + clamped.targetDepth);
        m_travelDepth = clamped.currentDepth;
        m_travelStartDepth = clampedStart;
        m_travelTargetDepth = clamped.targetDepth;
    }
    const ViewState view {
        .pan = m_pan,
        .viewport = {width(), height()},
        .zoom = m_zoom,
        .travelDepth = m_travelDepth,
    };
    m_layout = m_layoutEngine.layout(
        m_filesystemModel.nodes(), m_navigation.focusedId(), view);
    m_hovered = InvalidNodeId;
    update();
    if (perspectiveCameraChanged && notifyPerspectiveCameraChange)
        emit cameraChanged();
}

bool TreeSceneItem::usesPerspectiveProjection() const
{
    return m_layoutEngine.projectionKind() == ProjectionKind::Perspective;
}
