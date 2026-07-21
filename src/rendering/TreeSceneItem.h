#pragma once

#include "FilesystemTreeModel.h"
#include "NavigationState.h"
#include "TreeLayoutEngine.h"

#include <QHash>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QSet>
#include <QVariantAnimation>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

class QMouseEvent;
class QPainter;
class QPainterPath;
class QWheelEvent;

class TreeSceneItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TreeScene)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedType READ selectedType NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDetail READ selectedDetail NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedIsDirectory READ selectedIsDirectory NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDirectoryState READ selectedDirectoryState NOTIFY selectionChanged)
    Q_PROPERTY(qulonglong selectedTotalChildCount READ selectedTotalChildCount NOTIFY selectionChanged)
    Q_PROPERTY(qulonglong selectedMaterializedChildCount READ selectedMaterializedChildCount NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedRequiresContents READ selectedRequiresContents NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDirectoryStatus READ selectedDirectoryStatus NOTIFY selectionChanged)
    Q_PROPERTY(DirectoryContentsModel *contentsModel READ contentsModel CONSTANT)
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY navigationChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY cameraChanged)
    Q_PROPERTY(qreal travelDepth READ travelDepth NOTIFY cameraChanged)
    Q_PROPERTY(qreal maximumTravelDepth READ maximumTravelDepth NOTIFY cameraChanged)
    Q_PROPERTY(bool canTravelForward READ canTravelForward NOTIFY cameraChanged)
    Q_PROPERTY(bool canTravelBackward READ canTravelBackward NOTIFY cameraChanged)
    Q_PROPERTY(bool canResetView READ canResetView NOTIFY cameraChanged)
    Q_PROPERTY(bool perspectiveProjection READ usesPerspectiveProjection CONSTANT)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY navigationChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString pathMessage READ pathMessage NOTIFY pathMessageChanged)

public:
    explicit TreeSceneItem(QQuickItem *parent = nullptr);
    explicit TreeSceneItem(
        const QString &rootPath, QQuickItem *parent = nullptr);
    TreeSceneItem(
        const QString &rootPath,
        ProjectionKind projectionKind,
        QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    [[nodiscard]] QString selectedName() const;
    [[nodiscard]] QString selectedType() const;
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] QString selectedDetail() const;
    [[nodiscard]] bool selectedIsDirectory() const;
    [[nodiscard]] QString selectedDirectoryState() const;
    [[nodiscard]] qulonglong selectedTotalChildCount() const;
    [[nodiscard]] qulonglong selectedMaterializedChildCount() const;
    [[nodiscard]] bool selectedRequiresContents() const;
    [[nodiscard]] QString selectedDirectoryStatus() const;
    [[nodiscard]] DirectoryContentsModel *contentsModel() noexcept
    {
        return m_filesystemModel.contentsModel();
    }
    [[nodiscard]] QString breadcrumb() const;
    [[nodiscard]] QPointF pan() const noexcept { return m_pan; }
    [[nodiscard]] qreal zoom() const noexcept { return m_zoom; }
    [[nodiscard]] qreal travelDepth() const noexcept { return m_travelDepth; }
    [[nodiscard]] qreal maximumTravelDepth() const noexcept
    {
        return m_maximumTravelDepth;
    }
    [[nodiscard]] bool canTravelForward() const noexcept;
    [[nodiscard]] bool canTravelBackward() const noexcept;
    [[nodiscard]] bool canResetView() const noexcept;
    [[nodiscard]] bool usesPerspectiveProjection() const;
    [[nodiscard]] bool canGoBack() const noexcept { return m_navigation.canGoBack(); }
    [[nodiscard]] bool canGoUp() const;
    [[nodiscard]] bool reducedMotion() const noexcept { return m_reducedMotion; }
    void setReducedMotion(bool reducedMotion);
    [[nodiscard]] bool showHidden() const noexcept
    {
        return m_filesystemModel.includeHidden();
    }
    void setShowHidden(bool showHidden);
    [[nodiscard]] QString pathMessage() const { return m_pathMessage; }

    Q_INVOKABLE void resetView();
    Q_INVOKABLE void travelForward();
    Q_INVOKABLE void travelBackward();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void focusSelected();
    Q_INVOKABLE bool navigateToPath(const QString &path);
    Q_INVOKABLE void activateContentsRow(int row);

signals:
    void selectionChanged();
    void navigationChanged();
    void cameraChanged();
    void reducedMotionChanged();
    void showHiddenChanged();
    void pathMessageChanged();

protected:
    void geometryChange(
        const QRectF &newGeometry,
        const QRectF &oldGeometry) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct DepartureSnapshot {
        LayoutNode layout;
        NodeKind kind = NodeKind::Directory;
    };

    [[nodiscard]] QPointF toScene(const QPointF &viewPosition) const;
    [[nodiscard]] NodeId nodeAt(const QPointF &viewPosition) const;
    [[nodiscard]] QSet<NodeId> visibleLabelIds(
        qreal presentationZoom) const;
    [[nodiscard]] const FileNode *node(NodeId id) const;
    [[nodiscard]] const LayoutNode *layoutNode(NodeId id) const;
    [[nodiscard]] QPointF drawnPosition(const LayoutNode &node) const;
    [[nodiscard]] QPainterPath drawnBranchPath(const LayoutNode &node) const;
    [[nodiscard]] QPainterPath drawnCrownPath(const LayoutNode &node) const;
    [[nodiscard]] QString kindName(NodeKind kind) const;
    [[nodiscard]] QColor kindColor(NodeKind kind) const;
    void selectNode(NodeId id);
    void focusNode(NodeId id, bool addToHistory);
    void openFile(const QString &path);
    void setPathMessage(const QString &message);
    void zoomAt(const QPointF &viewPosition, qreal factor, bool recenter);
    [[nodiscard]] PerspectiveTravelRange perspectiveTravelRange() const;
    void finishTransientAnimationsForTravel();
    void freezePerspectiveTravel();
    void resetPerspectiveTravel();
    void startPerspectiveTravel(qreal targetDepth);
    void applyPerspectiveWheel(qreal angleDeltaY, qreal pixelDeltaY);
    void handleTreeChange(const TreeChangeSet &change);
    void applyTreeChange(
        const TreeChangeSet &change,
        bool notifyPerspectiveCameraChange = true);
    void relayout(bool notifyPerspectiveCameraChange = true);

    FilesystemTreeModel m_filesystemModel;
    QVector<LayoutNode> m_layout;
    QVector<DepartureSnapshot> m_departingLayout;
    QVector<DepartureSnapshot> m_liveDepartingLayout;
    TreeLayoutEngine m_layoutEngine;
    NavigationState m_navigation {1};
    QHash<NodeId, QPointF> m_previousPositions;
    QHash<NodeId, QPointF> m_livePreviousPositions;
    QSet<NodeId> m_enteringNodes;
    QSet<NodeId> m_liveEnteringNodes;
    QVariantAnimation m_focusAnimation;
    QVariantAnimation m_liveChangeAnimation;
    QVariantAnimation m_cameraAnimation;
    QVariantAnimation m_travelAnimation;
    QPointF m_pan {120.0, 35.0};
    QPointF m_cameraStartPan;
    QPointF m_cameraTargetPan;
    QPointF m_lastPointer;
    QString m_pathMessage;
    qreal m_zoom = 1.0;
    qreal m_cameraStartZoom = 1.0;
    qreal m_cameraTargetZoom = 1.0;
    qreal m_travelDepth = 0.0;
    qreal m_travelStartDepth = 0.0;
    qreal m_travelTargetDepth = 0.0;
    qreal m_maximumTravelDepth = 0.0;
    qreal m_transitionProgress = 1.0;
    qreal m_liveChangeProgress = 1.0;
    NodeId m_hovered = InvalidNodeId;
    bool m_panning = false;
    bool m_reducedMotion = false;
};
