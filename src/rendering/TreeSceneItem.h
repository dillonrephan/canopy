#pragma once

#include "FileNode.h"
#include "DirectoryContentsModel.h"
#include "DirectoryScanController.h"
#include "NavigationState.h"
#include "TreeLayoutEngine.h"

#include <QHash>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QStringList>
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
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY navigationChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)

public:
    explicit TreeSceneItem(QQuickItem *parent = nullptr);

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
        return &m_contentsModel;
    }
    [[nodiscard]] QString breadcrumb() const;
    [[nodiscard]] qreal zoom() const noexcept { return m_zoom; }
    [[nodiscard]] bool canGoBack() const noexcept { return m_navigation.canGoBack(); }
    [[nodiscard]] bool canGoUp() const;
    [[nodiscard]] bool reducedMotion() const noexcept { return m_reducedMotion; }
    void setReducedMotion(bool reducedMotion);

    Q_INVOKABLE void resetView();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void focusSelected();

signals:
    void selectionChanged();
    void navigationChanged();
    void cameraChanged();
    void reducedMotionChanged();

protected:
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    [[nodiscard]] QPointF toScene(const QPointF &viewPosition) const;
    [[nodiscard]] NodeId nodeAt(const QPointF &viewPosition) const;
    [[nodiscard]] const FileNode *node(NodeId id) const;
    [[nodiscard]] FileNode *mutableNode(NodeId id);
    [[nodiscard]] const LayoutNode *layoutNode(NodeId id) const;
    [[nodiscard]] QPointF drawnPosition(const LayoutNode &node) const;
    [[nodiscard]] QPainterPath drawnBranchPath(const LayoutNode &node) const;
    [[nodiscard]] QString kindName(NodeKind kind) const;
    [[nodiscard]] QColor kindColor(NodeKind kind) const;
    void selectNode(NodeId id);
    void focusNode(NodeId id, bool addToHistory);
    void scanDirectory(NodeId id);
    void applyScanUpdate(NodeId id, const DirectoryScanUpdate &update);
    void applyScanResult(NodeId id, const DirectoryScanResult &result);
    void appendScannedEntries(NodeId id, const QVector<ScannedEntry> &entries);
    void relayout();

    QVector<FileNode> m_nodes;
    QVector<LayoutNode> m_layout;
    TreeLayoutEngine m_layoutEngine;
    NavigationState m_navigation {1};
    DirectoryContentsModel m_contentsModel;
    DirectoryScanController m_scanController;
    QHash<NodeId, QPointF> m_previousPositions;
    QVariantAnimation m_focusAnimation;
    QPointF m_pan {120.0, 35.0};
    QPointF m_lastPointer;
    qreal m_zoom = 1.0;
    qreal m_transitionProgress = 1.0;
    NodeId m_hovered = InvalidNodeId;
    NodeId m_scanningNodeId = InvalidNodeId;
    NodeId m_nextNodeId = 2;
    quint64 m_activeScanRequest = 0;
    bool m_panning = false;
    bool m_reducedMotion = false;
};
