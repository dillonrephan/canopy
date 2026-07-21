#include "DirectoryPresentationPolicy.h"
#include "FakeTreeData.h"
#include "LabelVisibilityPolicy.h"
#include "NavigationState.h"
#include "PerspectiveProjection.h"
#include "ThemeRegistry.h"
#include "TreeLayoutEngine.h"
#include "WorldLayoutEngine.h"

#include <QSet>
#include <QtTest>

#include <algorithm>

namespace {
QVector<FileNode> perspectiveNodes(int childCount)
{
    QVector<FileNode> nodes;
    nodes.reserve(childCount + 1);
    nodes.append({
        .id = 1,
        .parentId = InvalidNodeId,
        .name = QStringLiteral("Focus"),
        .path = QStringLiteral("/focus"),
        .kind = NodeKind::Directory
    });
    for (int i = 0; i < childCount; ++i) {
        nodes.append({
            .id = static_cast<NodeId>(i + 2),
            .parentId = 1,
            .name = QStringLiteral("Child %1").arg(i),
            .path = QStringLiteral("/focus/child-%1").arg(i),
            .kind = NodeKind::Directory
        });
    }
    return nodes;
}

WorldLayout perspectiveWorld(int childCount)
{
    WorldLayout world {.focusedId = 1};
    world.nodes.append({
        .id = 1,
        .parentId = InvalidNodeId,
        .role = WorldNodeRole::Focus
    });
    for (int i = 0; i < childCount; ++i) {
        world.nodes.append({
            .id = static_cast<NodeId>(i + 2),
            .parentId = 1,
            .role = WorldNodeRole::DirectChild,
            .siblingOrder = i,
            .siblingCount = childCount,
            .lane = i,
            .semanticDepth = 1,
            .distanceAlongParent = i / 2.0 + 1.0
        });
    }
    return world;
}

ViewState perspectiveView(qreal travelDepth = 0.0)
{
    return {
        .viewport = {1000.0, 800.0},
        .travelDepth = travelDepth
    };
}

const ProjectedNode *projectedNode(
    const QVector<ProjectedNode> &nodes, NodeId id)
{
    const auto found = std::ranges::find_if(
        nodes, [id](const ProjectedNode &node) {
            return node.id == id;
        });
    return found == nodes.cend() ? nullptr : &*found;
}

bool approximately(qreal actual, qreal expected, qreal tolerance = 0.01)
{
    return qAbs(actual - expected) <= tolerance;
}
}

class TreeLayoutEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureHasStableRelationships();
    void activeThemeProfileIsComplete();
    void worldPlacementMatchesPlanarLanes();
    void worldPlacementRemainsStableWhenChildIsAdded();
    void planarProjectionPreservesGeometryAcrossViewState();
    void perspectiveProjectionMatchesKnownCoordinates();
    void perspectiveTravelIsMonotonicAndReversible();
    void perspectiveProjectionCullsPassedGeometry();
    void perspectiveProjectionPreservesLaneOrdering();
    void perspectiveProjectionIsFiniteAndBounded();
    void layoutIsDeterministic();
    void focusedDirectoryKeepsParentContext();
    void wideDirectoryUsesUniquePositions();
    void wideDirectoryReservesLabelSpace();
    void homeDirectChildrenReserveLabelSpace();
    void normalZoomPrioritizesHomeLevelLabels();
    void lowZoomKeepsFocusAndEmphasizedLabels();
    void closeZoomRevealsOnlyNonOverlappingDescendants();
    void zoomClearsRearCorridorFog();
    void navigationStateTracksSelectionAndHistory();
    void layoutProvidesConnectedBranchGeometry();
    void directChildrenUseStaggeredCorridors();
    void corridorsRemainStableWhenChildIsAdded();
    void busyDirectoriesFadeRearCorridors();
    void fixtureTracksDirectoryCountsAndStates();
    void presentationPolicyBoundsExtremeDirectory();
    void layoutBoundsMaterializedExtremeChildren();
};

void TreeLayoutEngineTest::fixtureHasStableRelationships()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    QVERIFY(nodes.size() >= 30);
    QCOMPARE(nodes.first().id, NodeId(1));
    QCOMPARE(nodes.first().parentId, InvalidNodeId);
    QCOMPARE(nodes.first().path, QStringLiteral("/home/dillon"));
    QCOMPARE(nodes.first().children.size(), 5);

    for (const FileNode &node : nodes) {
        QCOMPARE(node.id, NodeId(&node - nodes.constData() + 1));
        if (node.parentId != InvalidNodeId)
            QVERIFY(node.path.startsWith(QStringLiteral("/home/dillon/")));
    }
}

void TreeLayoutEngineTest::activeThemeProfileIsComplete()
{
    const ThemeProfile &theme = ThemeRegistry::active();

    QCOMPARE(theme.id, QStringLiteral("sakura-morning"));
    QVERIFY(!theme.name.isEmpty());
    QVERIFY(theme.palette.sceneCanvas.isValid());
    QVERIFY(theme.palette.textDark.isValid());
    QVERIFY(theme.palette.textLight.isValid());
    QVERIFY(theme.branches.focusedWidth > theme.branches.directoryWidth);
    QVERIFY(theme.camera.minimumZoom < 1.0);
    QVERIFY(theme.camera.maximumZoom > 1.0);
    QVERIFY(theme.camera.fogClearingZoom <= theme.camera.maximumZoom);
    QCOMPARE(theme.planar.primaryCorridorCount,
             theme.presentation.foregroundCorridorCount);
    QCOMPARE(theme.planar.corridorCount,
             theme.presentation.foregroundCorridorCount
                 + theme.presentation.fogHintCount);
    QCOMPARE(theme.perspective.foregroundCorridorCount, 6);
    QCOMPARE(theme.perspective.fogHintCount, 2);
    QVERIFY(theme.perspective.fogStartDepth
            < theme.perspective.fogEndDepth);
    QVERIFY(theme.perspective.minimumCameraDepth > 0.0);
}

void TreeLayoutEngineTest::worldPlacementMatchesPlanarLanes()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const WorldLayoutEngine worldEngine;
    const TreeLayoutEngine planarEngine;
    const WorldLayout world = worldEngine.layout(nodes, 1);
    const QVector<LayoutNode> planar = planarEngine.layout(nodes, 1);

    QCOMPARE(world.focusedId, NodeId(1));
    QCOMPARE(world.nodes.size(), planar.size());
    for (const WorldNodePlacement &placement : world.nodes) {
        const auto projected = std::ranges::find_if(
            planar, [&placement](const LayoutNode &node) {
                return node.id == placement.id;
            });
        QVERIFY(projected != planar.cend());
        if (placement.role == WorldNodeRole::DirectChild) {
            QCOMPARE(placement.semanticDepth, 1);
            QVERIFY(placement.distanceAlongParent > 0.0);
            QCOMPARE(placement.lane, projected->corridorSlot);
        }
    }
}

void TreeLayoutEngineTest::worldPlacementRemainsStableWhenChildIsAdded()
{
    QVector<FileNode> nodes = FakeTreeData::create();
    WorldLayoutEngine engine;
    const WorldLayout before = engine.layout(nodes, 1);
    QHash<NodeId, WorldNodePlacement> originalChildren;
    for (const WorldNodePlacement &placement : before.nodes) {
        if (placement.role == WorldNodeRole::DirectChild)
            originalChildren.insert(placement.id, placement);
    }

    const NodeId addedId = static_cast<NodeId>(nodes.size() + 1);
    nodes.first().children.append(addedId);
    ++nodes.first().totalChildCount;
    nodes.append({
        .id = addedId,
        .parentId = 1,
        .name = QStringLiteral("New Growth"),
        .path = QStringLiteral("/home/dillon/New Growth"),
        .kind = NodeKind::Directory
    });

    const WorldLayout after = engine.layout(nodes, 1);
    for (const WorldNodePlacement &placement : after.nodes) {
        if (!originalChildren.contains(placement.id))
            continue;
        const WorldNodePlacement original =
            originalChildren.value(placement.id);
        QCOMPARE(placement.lane, original.lane);
        QCOMPARE(placement.distanceAlongParent,
                 original.distanceAlongParent);
        QCOMPARE(placement.semanticDepth, original.semanticDepth);
    }
}

void TreeLayoutEngineTest::planarProjectionPreservesGeometryAcrossViewState()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const WorldLayoutEngine worldEngine;
    const PlanarProjection projection;
    const WorldLayout world = worldEngine.layout(nodes, 1);
    const QVector<ProjectedNode> defaultView =
        projection.project(world, nodes, {});
    const QVector<ProjectedNode> movedView = projection.project(
        world,
        nodes,
        {
            .pan = {-800.0, 420.0},
            .viewport = {2560.0, 1440.0},
            .zoom = 4.5,
            .travelDepth = 12.0
        });

    QCOMPARE(defaultView.size(), movedView.size());
    for (qsizetype i = 0; i < defaultView.size(); ++i) {
        QCOMPARE(defaultView.at(i).id, movedView.at(i).id);
        QCOMPARE(defaultView.at(i).position, movedView.at(i).position);
        QCOMPARE(defaultView.at(i).branchStart,
                 movedView.at(i).branchStart);
        QCOMPARE(defaultView.at(i).branchControl1,
                 movedView.at(i).branchControl1);
        QCOMPARE(defaultView.at(i).branchControl2,
                 movedView.at(i).branchControl2);
        QCOMPARE(defaultView.at(i).labelBounds,
                 movedView.at(i).labelBounds);
    }
}

void TreeLayoutEngineTest::perspectiveProjectionMatchesKnownCoordinates()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(1);
    WorldLayout nearWorld = perspectiveWorld(1);
    const QVector<ProjectedNode> nearProjection = projection.project(
        nearWorld, nodes, perspectiveView());
    const ProjectedNode *near = projectedNode(nearProjection, 2);
    QVERIFY(near);
    QVERIFY(approximately(near->position.x(), 385.7143));
    QVERIFY(approximately(near->position.y(), 523.4286));
    QVERIFY(approximately(near->cameraDepth, 2.0));
    QVERIFY(approximately(near->apparentScale, 5.0 / 7.0));
    QVERIFY(approximately(near->radius, 15.0));
    QVERIFY(approximately(near->depthOpacity, 1.0));

    WorldLayout farWorld = perspectiveWorld(1);
    farWorld.nodes[1].lane = 8;
    farWorld.nodes[1].distanceAlongParent = 5.0;
    const QVector<ProjectedNode> farProjection = projection.project(
        farWorld, nodes, perspectiveView());
    const ProjectedNode *far = projectedNode(farProjection, 2);
    QVERIFY(far);
    QVERIFY(approximately(far->position.x(), 136.3636));
    QVERIFY(approximately(far->position.y(), 402.9091));
    QVERIFY(approximately(far->cameraDepth, 6.0));
    QVERIFY(approximately(far->apparentScale, 5.0 / 11.0));
    QVERIFY(approximately(far->depthOpacity, 0.70));
}

void TreeLayoutEngineTest::perspectiveTravelIsMonotonicAndReversible()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(1);
    const WorldLayout world = perspectiveWorld(1);
    const QVector<ProjectedNode> initial = projection.project(
        world, nodes, perspectiveView());
    const QVector<ProjectedNode> approached = projection.project(
        world, nodes, perspectiveView(0.75));
    const QVector<ProjectedNode> restored = projection.project(
        world, nodes, perspectiveView());
    const ProjectedNode *initialChild = projectedNode(initial, 2);
    const ProjectedNode *approachedChild = projectedNode(approached, 2);
    const ProjectedNode *restoredChild = projectedNode(restored, 2);
    QVERIFY(initialChild);
    QVERIFY(approachedChild);
    QVERIFY(restoredChild);

    QVERIFY(approachedChild->position.y() > initialChild->position.y());
    QVERIFY(qAbs(approachedChild->position.x() - 500.0)
            > qAbs(initialChild->position.x() - 500.0));
    QVERIFY(approachedChild->radius > initialChild->radius);
    QVERIFY(approachedChild->depthOpacity >= initialChild->depthOpacity);
    QVERIFY(approximately(approachedChild->position.x(), 372.0));
    QVERIFY(approximately(approachedChild->position.y(), 563.2));
    QVERIFY(approximately(approachedChild->apparentScale, 0.8));

    QCOMPARE(restoredChild->position, initialChild->position);
    QCOMPARE(restoredChild->radius, initialChild->radius);
    QCOMPARE(restoredChild->cameraDepth, initialChild->cameraDepth);
    QCOMPARE(restoredChild->apparentScale,
             initialChild->apparentScale);
    QCOMPARE(restoredChild->depthOpacity,
             initialChild->depthOpacity);
}

void TreeLayoutEngineTest::perspectiveProjectionCullsPassedGeometry()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(1);
    const WorldLayout world = perspectiveWorld(1);

    const QVector<ProjectedNode> pastFocus = projection.project(
        world, nodes, perspectiveView(1.1));
    QVERIFY(!projectedNode(pastFocus, 1));
    QVERIFY(projectedNode(pastFocus, 2));

    const QVector<ProjectedNode> pastEverything = projection.project(
        world, nodes, perspectiveView(2.1));
    QVERIFY(pastEverything.isEmpty());
}

void TreeLayoutEngineTest::perspectiveProjectionPreservesLaneOrdering()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(2);
    WorldLayout world = perspectiveWorld(2);
    world.nodes[1].distanceAlongParent = 1.0;
    world.nodes[2].distanceAlongParent = 1.0;

    const QVector<ProjectedNode> initial = projection.project(
        world, nodes, perspectiveView());
    const QVector<ProjectedNode> approached = projection.project(
        world, nodes, perspectiveView(0.75));
    const ProjectedNode *left = projectedNode(initial, 2);
    const ProjectedNode *right = projectedNode(initial, 3);
    const ProjectedNode *approachedLeft = projectedNode(approached, 2);
    const ProjectedNode *approachedRight = projectedNode(approached, 3);
    QVERIFY(left);
    QVERIFY(right);
    QVERIFY(approachedLeft);
    QVERIFY(approachedRight);

    QVERIFY(left->position.x() < 500.0);
    QVERIFY(right->position.x() > 500.0);
    QVERIFY(approximately(left->position.y(), right->position.y()));
    QVERIFY(approachedLeft->position.x() < left->position.x());
    QVERIFY(approachedRight->position.x() > right->position.x());
}

void TreeLayoutEngineTest::perspectiveProjectionIsFiniteAndBounded()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(16);
    const WorldLayout world = perspectiveWorld(16);
    const QVector<ProjectedNode> projected = projection.project(
        world, nodes, perspectiveView());
    qsizetype directChildren = 0;
    qsizetype fogHints = 0;

    for (const ProjectedNode &node : projected) {
        QVERIFY(qIsFinite(node.position.x()));
        QVERIFY(qIsFinite(node.position.y()));
        QVERIFY(qIsFinite(node.radius));
        QVERIFY(qIsFinite(node.cameraDepth));
        QVERIFY(qIsFinite(node.apparentScale));
        if (node.parentId == NodeId(1)) {
            ++directChildren;
            if (node.fogHint)
                ++fogHints;
        }
    }
    QCOMPARE(directChildren, qsizetype(8));
    QCOMPARE(fogHints, qsizetype(2));

    const QVector<ProjectedNode> extremeDistance = projection.project(
        world, nodes, perspectiveView(-1'000'000.0));
    for (const ProjectedNode &node : extremeDistance) {
        QVERIFY(qIsFinite(node.position.x()));
        QVERIFY(qIsFinite(node.position.y()));
        QVERIFY(qIsFinite(node.apparentScale));
    }
    const QVector<ProjectedNode> farPast = projection.project(
        world, nodes, perspectiveView(1'000'000.0));
    QVERIFY(farPast.isEmpty());
}

void TreeLayoutEngineTest::layoutIsDeterministic()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const QVector<LayoutNode> first = engine.layout(nodes, 1);
    const QVector<LayoutNode> second = engine.layout(nodes, 1);

    QCOMPARE(first.size(), second.size());
    for (qsizetype i = 0; i < first.size(); ++i) {
        QCOMPARE(first.at(i).id, second.at(i).id);
        QCOMPARE(first.at(i).parentId, second.at(i).parentId);
        QCOMPARE(first.at(i).position, second.at(i).position);
        QCOMPARE(first.at(i).labelBounds, second.at(i).labelBounds);
        QCOMPARE(first.at(i).branchStart, second.at(i).branchStart);
        QCOMPARE(first.at(i).branchControl1, second.at(i).branchControl1);
        QCOMPARE(first.at(i).branchControl2, second.at(i).branchControl2);
        QCOMPARE(first.at(i).hiddenChildCount, second.at(i).hiddenChildCount);
    }
}

void TreeLayoutEngineTest::focusedDirectoryKeepsParentContext()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId projectsId = 6;
    const QVector<LayoutNode> layout = engine.layout(nodes, projectsId);

    QVERIFY(layout.size() >= 3);
    QCOMPARE(layout.at(0).id, NodeId(1));
    QVERIFY(layout.at(0).parentContext);
    QCOMPARE(layout.at(1).id, projectsId);
    QCOMPARE(layout.at(1).parentId, NodeId(1));
}

void TreeLayoutEngineTest::wideDirectoryUsesUniquePositions()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId downloadsId = 5;
    const QVector<LayoutNode> layout = engine.layout(nodes, downloadsId);
    QSet<QString> positions;

    QCOMPARE(layout.size(), 16); // parent context + focus + 14 files
    for (const LayoutNode &node : layout) {
        const QString key = QStringLiteral("%1,%2")
                                .arg(node.position.x(), 0, 'f', 2)
                                .arg(node.position.y(), 0, 'f', 2);
        QVERIFY2(!positions.contains(key), qPrintable(key));
        positions.insert(key);
    }
}

void TreeLayoutEngineTest::wideDirectoryReservesLabelSpace()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId downloadsId = 5;
    const QVector<LayoutNode> layout = engine.layout(nodes, downloadsId);
    QVector<QRectF> labelBounds;

    for (const LayoutNode &node : layout) {
        if (node.parentId != downloadsId)
            continue;

        QVERIFY(!node.labelBounds.isEmpty());
        for (const QRectF &existing : labelBounds) {
            QVERIFY2(!node.labelBounds.intersects(existing),
                     qPrintable(QStringLiteral("Overlapping labels at (%1, %2) and (%3, %4)")
                                    .arg(node.labelBounds.x())
                                    .arg(node.labelBounds.y())
                                    .arg(existing.x())
                                    .arg(existing.y())));
        }
        labelBounds.append(node.labelBounds);
    }
}

void TreeLayoutEngineTest::homeDirectChildrenReserveLabelSpace()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId homeId = 1;
    const QVector<LayoutNode> layout = engine.layout(nodes, homeId);
    QVector<QRectF> labelBounds;

    for (const LayoutNode &node : layout) {
        if (node.parentId != homeId)
            continue;

        for (const QRectF &existing : labelBounds)
            QVERIFY2(!node.labelBounds.intersects(existing),
                     "Home directory labels must not overlap");
        labelBounds.append(node.labelBounds);
    }

    QCOMPARE(labelBounds.size(), 5);
}

void TreeLayoutEngineTest::normalZoomPrioritizesHomeLevelLabels()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId homeId = 1;
    const QVector<LayoutNode> layout = engine.layout(nodes, homeId);
    const QSet<NodeId> visible = LabelVisibilityPolicy::visibleLabels(
        layout, homeId, homeId, InvalidNodeId, 1.0);

    for (const LayoutNode &node : layout) {
        const bool homeLevel = node.id == homeId || node.parentId == homeId;
        QCOMPARE(visible.contains(node.id), homeLevel);
    }
}

void TreeLayoutEngineTest::lowZoomKeepsFocusAndEmphasizedLabels()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId homeId = 1;
    const NodeId selectedGrandchildId = 7;
    const NodeId hoveredGrandchildId = 11;
    const QVector<LayoutNode> layout = engine.layout(nodes, homeId);
    const QSet<NodeId> visible = LabelVisibilityPolicy::visibleLabels(
        layout, homeId, selectedGrandchildId, hoveredGrandchildId, 0.6);

    QCOMPARE(visible, QSet<NodeId>({homeId, selectedGrandchildId, hoveredGrandchildId}));
}

void TreeLayoutEngineTest::closeZoomRevealsOnlyNonOverlappingDescendants()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId homeId = 1;
    const QVector<LayoutNode> layout = engine.layout(nodes, homeId);
    const QSet<NodeId> visible = LabelVisibilityPolicy::visibleLabels(
        layout, homeId, homeId, InvalidNodeId, 1.4);
    QVector<QRectF> occupiedBounds;
    qsizetype visibleDescendants = 0;

    for (const LayoutNode &node : layout) {
        if (!visible.contains(node.id))
            continue;
        for (const QRectF &occupied : occupiedBounds)
            QVERIFY2(!node.labelBounds.intersects(occupied),
                     "Visible semantic-zoom labels must not overlap");
        occupiedBounds.append(node.labelBounds);
        if (node.id != homeId && node.parentId != homeId)
            ++visibleDescendants;
    }

    QVERIFY(visibleDescendants > 0);
}

void TreeLayoutEngineTest::zoomClearsRearCorridorFog()
{
    QCOMPARE(LabelVisibilityPolicy::depthOpacity(0.32, 1.0), qreal(0.32));
    QVERIFY(LabelVisibilityPolicy::depthOpacity(0.32, 2.0) > 0.55);
    QCOMPARE(LabelVisibilityPolicy::depthOpacity(0.32, 3.2), qreal(1.0));
    QCOMPARE(LabelVisibilityPolicy::depthOpacity(0.32, 5.0), qreal(1.0));
}

void TreeLayoutEngineTest::navigationStateTracksSelectionAndHistory()
{
    NavigationState state(1);

    QCOMPARE(state.focusedId(), NodeId(1));
    QCOMPARE(state.selectedId(), NodeId(1));
    QVERIFY(!state.canGoBack());

    QVERIFY(state.select(6));
    QCOMPARE(state.focusedId(), NodeId(1));
    QCOMPARE(state.selectedId(), NodeId(6));

    QVERIFY(state.focus(6, true));
    QCOMPARE(state.focusedId(), NodeId(6));
    QCOMPARE(state.selectedId(), NodeId(6));
    QVERIFY(state.canGoBack());
    QCOMPARE(state.takeBackTarget(), NodeId(1));
    QVERIFY(!state.canGoBack());

    QVERIFY(state.focus(1, false));
    QCOMPARE(state.focusedId(), NodeId(1));
    QCOMPARE(state.selectedId(), NodeId(1));
}

void TreeLayoutEngineTest::layoutProvidesConnectedBranchGeometry()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId homeId = 1;
    const QVector<LayoutNode> layout = engine.layout(nodes, homeId);

    for (const LayoutNode &node : layout) {
        QVERIFY2(node.branchStart != node.position,
                 "Every visible item needs a non-empty growth path");
        QVERIFY(node.branchControl1 != QPointF());
        QVERIFY(node.branchControl2 != QPointF());

        if (node.id == homeId) {
            QCOMPARE(node.branchStart, QPointF(420.0, 650.0));
            continue;
        }

        const auto parent = std::ranges::find_if(
            layout, [&node](const LayoutNode &candidate) {
                return candidate.id == node.parentId;
            });
        QVERIFY(parent != layout.cend());
        if (node.followsParentPosition) {
            QCOMPARE(node.branchStart, parent->position);
        } else {
            QCOMPARE(node.branchStart.x(), parent->position.x());
            QVERIFY(node.branchStart.y() < parent->position.y());
            QVERIFY(node.branchStart.y() >= parent->crownEnd.y());
        }
    }
}

void TreeLayoutEngineTest::directChildrenUseStaggeredCorridors()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const QVector<LayoutNode> layout = engine.layout(nodes, 1);
    const auto focused = std::ranges::find_if(
        layout, [](const LayoutNode &node) { return node.id == NodeId(1); });
    QVERIFY(focused != layout.cend());
    QVERIFY(focused->crownEnd.y() < focused->position.y());

    QSet<int> corridorSlots;
    QSet<qreal> attachmentHeights;
    for (const LayoutNode &node : layout) {
        if (node.parentId != NodeId(1))
            continue;
        QVERIFY(!node.followsParentPosition);
        QVERIFY(node.corridorSlot >= 0);
        QVERIFY(node.depthLayer >= 1);
        QCOMPARE(node.depthOpacity, qreal(1.0));
        corridorSlots.insert(node.corridorSlot);
        attachmentHeights.insert(node.branchStart.y());
    }

    QCOMPARE(corridorSlots.size(), 5);
    QCOMPARE(attachmentHeights.size(), 5);
}

void TreeLayoutEngineTest::corridorsRemainStableWhenChildIsAdded()
{
    QVector<FileNode> nodes = FakeTreeData::create();
    TreeLayoutEngine engine;
    const QVector<LayoutNode> before = engine.layout(nodes, 1);
    QHash<NodeId, LayoutNode> originalChildren;
    for (const LayoutNode &node : before) {
        if (node.parentId == NodeId(1))
            originalChildren.insert(node.id, node);
    }

    const NodeId addedId = static_cast<NodeId>(nodes.size() + 1);
    nodes.first().children.append(addedId);
    ++nodes.first().totalChildCount;
    nodes.append({
        .id = addedId,
        .parentId = 1,
        .name = QStringLiteral("New Growth"),
        .path = QStringLiteral("/home/dillon/New Growth"),
        .kind = NodeKind::Directory,
        .detail = QStringLiteral("Live addition")
    });

    const QVector<LayoutNode> after = engine.layout(nodes, 1);
    for (const LayoutNode &node : after) {
        if (!originalChildren.contains(node.id))
            continue;
        const LayoutNode original = originalChildren.value(node.id);
        QCOMPARE(node.position, original.position);
        QCOMPARE(node.branchStart, original.branchStart);
        QCOMPARE(node.corridorSlot, original.corridorSlot);
    }
}

void TreeLayoutEngineTest::busyDirectoriesFadeRearCorridors()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine;
    const NodeId downloadsId = 5;
    const QVector<LayoutNode> layout = engine.layout(nodes, downloadsId);
    qsizetype primaryCorridors = 0;
    qsizetype fadedCorridors = 0;

    for (const LayoutNode &node : layout) {
        if (node.parentId != downloadsId)
            continue;
        if (node.depthOpacity >= 0.99)
            ++primaryCorridors;
        if (node.depthOpacity < 0.55)
            ++fadedCorridors;
    }

    QCOMPARE(primaryCorridors, qsizetype(8));
    QVERIFY(fadedCorridors >= 2);
}

void TreeLayoutEngineTest::fixtureTracksDirectoryCountsAndStates()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    QVERIFY(nodes.size() < 100);

    const FileNode &home = nodes.first();
    QCOMPARE(home.totalChildCount, quint64(home.children.size()));
    QCOMPARE(home.directoryState, DirectoryLoadState::Ready);

    const auto largeDataset = std::ranges::find_if(nodes, [](const FileNode &node) {
        return node.name == QStringLiteral("Large Dataset");
    });
    QVERIFY(largeDataset != nodes.cend());
    QVERIFY(largeDataset->isDirectory());
    QCOMPARE(largeDataset->totalChildCount, quint64(1'000'000));
    QCOMPARE(largeDataset->children.size(),
             DirectoryPresentationPolicy::VisualChildLimit);
    QCOMPARE(largeDataset->directoryState, DirectoryLoadState::Partial);
    QVERIFY(!largeDataset->directoryStatus.isEmpty());
}

void TreeLayoutEngineTest::presentationPolicyBoundsExtremeDirectory()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const auto largeDataset = std::ranges::find_if(nodes, [](const FileNode &node) {
        return node.name == QStringLiteral("Large Dataset");
    });
    QVERIFY(largeDataset != nodes.cend());

    const DirectoryPresentation largePresentation =
        DirectoryPresentationPolicy::evaluate(*largeDataset);
    QCOMPARE(largePresentation.visibleChildCount,
             DirectoryPresentationPolicy::VisualChildLimit);
    QCOMPARE(
        largePresentation.hiddenChildCount,
        quint64(1'000'000 - DirectoryPresentationPolicy::VisualChildLimit));
    QVERIFY(largePresentation.usesContentsFallback);

    const FileNode &downloads = nodes.at(4);
    const DirectoryPresentation normalPresentation =
        DirectoryPresentationPolicy::evaluate(downloads);
    QCOMPARE(normalPresentation.visibleChildCount, qsizetype(14));
    QCOMPARE(normalPresentation.hiddenChildCount, quint64(0));
    QVERIFY(!normalPresentation.usesContentsFallback);
}

void TreeLayoutEngineTest::layoutBoundsMaterializedExtremeChildren()
{
    QVector<FileNode> nodes;
    FileNode root {
        .id = 1,
        .parentId = InvalidNodeId,
        .name = QStringLiteral("Extreme"),
        .path = QStringLiteral("/extreme"),
        .kind = NodeKind::Directory,
        .detail = QStringLiteral("Synthetic stress fixture"),
        .totalChildCount = 1'000'000,
        .directoryState = DirectoryLoadState::Partial
    };

    constexpr qsizetype materializedCount = 100;
    root.children.reserve(materializedCount);
    nodes.reserve(materializedCount + 1);
    nodes.append(root);
    for (qsizetype i = 0; i < materializedCount; ++i) {
        const NodeId id = static_cast<NodeId>(i + 2);
        nodes.first().children.append(id);
        nodes.append({
            .id = id,
            .parentId = 1,
            .name = QStringLiteral("entry-%1").arg(i),
            .path = QStringLiteral("/extreme/entry-%1").arg(i),
            .kind = NodeKind::Document
        });
    }

    const TreeLayoutEngine engine;
    const QVector<LayoutNode> layout = engine.layout(nodes, 1);

    QCOMPARE(layout.size(), DirectoryPresentationPolicy::VisualChildLimit + 1);
    QCOMPARE(
        layout.first().hiddenChildCount,
        quint64(1'000'000 - DirectoryPresentationPolicy::VisualChildLimit));
}

QTEST_GUILESS_MAIN(TreeLayoutEngineTest)

#include "TreeLayoutEngineTest.moc"
