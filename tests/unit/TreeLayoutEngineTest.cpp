#include "DirectoryPresentationPolicy.h"
#include "FakeTreeData.h"
#include "LabelVisibilityPolicy.h"
#include "NavigationState.h"
#include "PerspectivePresentationPolicy.h"
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
        const NodeId childId = static_cast<NodeId>(i + 2);
        nodes.first().children.append(childId);
        nodes.append({
            .id = childId,
            .parentId = 1,
            .name = QStringLiteral("Child %1").arg(i),
            .path = QStringLiteral("/focus/child-%1").arg(i),
            .kind = NodeKind::Directory
        });
    }
    nodes.first().totalChildCount =
        static_cast<quint64>(childCount);
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

struct PerspectiveHierarchy {
    QVector<FileNode> nodes;
    WorldLayout world {.focusedId = 1};
};

PerspectiveHierarchy perspectiveHierarchy(
    int directChildCount, int descendantsPerChild)
{
    PerspectiveHierarchy fixture;
    fixture.nodes = perspectiveNodes(directChildCount);
    fixture.world = perspectiveWorld(directChildCount);
    NodeId descendantId =
        static_cast<NodeId>(directChildCount + 2);

    for (int directIndex = 0;
         directIndex < directChildCount; ++directIndex) {
        const NodeId parentId = static_cast<NodeId>(directIndex + 2);
        for (int descendantIndex = 0;
             descendantIndex < descendantsPerChild;
             ++descendantIndex) {
            fixture.nodes.append({
                .id = descendantId,
                .parentId = parentId,
                .name = QStringLiteral("Descendant %1")
                    .arg(descendantId),
                .path = QStringLiteral("/focus/child-%1/descendant-%2")
                    .arg(directIndex)
                    .arg(descendantIndex),
                .kind = NodeKind::Document
            });
            fixture.world.nodes.append({
                .id = descendantId,
                .parentId = parentId,
                .role = WorldNodeRole::Descendant,
                .siblingOrder = descendantIndex,
                .siblingCount = descendantsPerChild,
                .semanticDepth = 2,
                .distanceAlongParent = 1.0
            });
            ++descendantId;
        }
    }
    return fixture;
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
    void layoutEngineDispatchesSelectedProjection();
    void planarProjectionPreservesGeometryAcrossViewState();
    void perspectiveProjectionMatchesKnownCoordinates();
    void perspectiveTravelIsMonotonicAndReversible();
    void perspectiveProjectionCullsPassedGeometry();
    void perspectiveProjectionKeepsParentContextVisible();
    void perspectiveProjectionPreservesLaneOrdering();
    void perspectiveProjectionIsFiniteAndBounded();
    void perspectiveBudgetHasExactEdges();
    void perspectiveProjectionEnforcesHierarchyBudget();
    void perspectiveNonRootBudgetHasExactCeiling();
    void perspectiveWindowSlidesAndRestores();
    void perspectiveWindowUsesDeterministicOrdering();
    void perspectiveOverflowReflectsProjectionBudget();
    void perspectiveContentsPolicyUsesEightEntryBudget();
    void perspectiveProjectionUsesDepthOrder();
    void perspectiveProjectionTracksViewport();
    void perspectiveProjectionBuildsTaperedSurfaces();
    void perspectiveHitGeometryContainsPaintedSurfaces();
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
    QCOMPARE(theme.projection, ProjectionKind::Perspective);
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
    QVERIFY(theme.perspective.minimumFogOpacity
            >= theme.perspective.minimumHitOpacity);
    QVERIFY(theme.perspective.fogHintOpacity
            > theme.perspective.minimumHitOpacity);
    QVERIFY(theme.perspective.focusedTrunkNearWidth
            > theme.perspective.focusedTrunkFarWidth);
    QVERIFY(theme.perspective.directoryCorridorWidth
            > theme.perspective.fileCorridorWidth);
    QVERIFY(theme.perspective.hitPadding > 0.0);
}

void TreeLayoutEngineTest::worldPlacementMatchesPlanarLanes()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const WorldLayoutEngine worldEngine;
    const TreeLayoutEngine planarEngine(ProjectionKind::Planar);
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

void TreeLayoutEngineTest::layoutEngineDispatchesSelectedProjection()
{
    const QVector<FileNode> nodes = perspectiveNodes(8);
    const ViewState view = perspectiveView();
    const TreeLayoutEngine themeEngine;
    const TreeLayoutEngine perspectiveEngine(ProjectionKind::Perspective);
    const TreeLayoutEngine planarEngine(ProjectionKind::Planar);

    QCOMPARE(themeEngine.projectionKind(), ProjectionKind::Perspective);
    QCOMPARE(planarEngine.projectionKind(), ProjectionKind::Planar);

    const QVector<ProjectedNode> selected =
        themeEngine.layout(nodes, 1, view);
    const QVector<ProjectedNode> perspective =
        perspectiveEngine.layout(nodes, 1, view);
    const QVector<ProjectedNode> planar =
        planarEngine.layout(nodes, 1, view);

    QCOMPARE(selected.size(), perspective.size());
    QVERIFY(!selected.isEmpty());
    QVERIFY(!selected.first().branchSurface.isEmpty());
    QCOMPARE(selected.first().position, perspective.first().position);
    QVERIFY(planar.first().branchSurface.isEmpty());
    QVERIFY(planar.first().hitArea.isEmpty());
    QVERIFY(planar.first().position != selected.first().position);
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

void TreeLayoutEngineTest::perspectiveProjectionKeepsParentContextVisible()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const WorldLayout world = WorldLayoutEngine().layout(nodes, 6);
    const QVector<ProjectedNode> projected =
        PerspectiveProjection().project(world, nodes, perspectiveView());
    const ProjectedNode *parent = projectedNode(projected, 1);
    const ProjectedNode *focus = projectedNode(projected, 6);
    QVERIFY(parent);
    QVERIFY(focus);

    QVERIFY(parent->parentContext);
    QCOMPARE(
        parent->cameraDepth,
        ThemeRegistry::active().perspective.cameraClearance * 0.5);
    QVERIFY(parent->cameraDepth < focus->cameraDepth);
    QVERIFY(parent->position.y() > focus->position.y());
    QVERIFY(!parent->branchSurface.isEmpty());
    QVERIFY(!parent->hitArea.isEmpty());
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
        if (node.fogHint) {
            QCOMPARE(node.depthOpacity,
                     ThemeRegistry::active()
                         .perspective.minimumFogOpacity);
            QVERIFY(node.depthOpacity
                    >= ThemeRegistry::active()
                        .perspective.minimumHitOpacity);
            QVERIFY(!node.hitArea.isEmpty());
        }
    }
    const QVector<ProjectedNode> farPast = projection.project(
        world, nodes, perspectiveView(1'000'000.0));
    QVERIFY(farPast.isEmpty());
}

void TreeLayoutEngineTest::perspectiveBudgetHasExactEdges()
{
    const PerspectiveProjection projection;
    for (const int childCount : {0, 1, 5, 6, 7, 8, 9, 16}) {
        const QVector<ProjectedNode> projected = projection.project(
            perspectiveWorld(childCount),
            perspectiveNodes(childCount),
            perspectiveView());
        qsizetype foregroundCount = 0;
        qsizetype fogHintCount = 0;
        for (const ProjectedNode &node : projected) {
            if (node.parentId != NodeId(1))
                continue;
            if (node.fogHint)
                ++fogHintCount;
            else
                ++foregroundCount;
        }

        const qsizetype expectedForeground = qMin(childCount, 6);
        const qsizetype expectedFogHints =
            qMin(qMax(childCount - 6, 0), 2);
        const qsizetype expectedDirect = qMin(childCount, 8);
        QCOMPARE(foregroundCount, expectedForeground);
        QCOMPARE(fogHintCount, expectedFogHints);
        QCOMPARE(foregroundCount + fogHintCount, expectedDirect);
        QCOMPARE(projected.size(), qsizetype(1) + expectedDirect);
    }
}

void TreeLayoutEngineTest::perspectiveProjectionEnforcesHierarchyBudget()
{
    constexpr int directChildCount = 16;
    constexpr int descendantsPerChild = 2;
    const PerspectiveHierarchy fixture = perspectiveHierarchy(
        directChildCount, descendantsPerChild);
    const PerspectiveProjection projection;
    const QVector<ProjectedNode> first = projection.project(
        fixture.world, fixture.nodes, perspectiveView());
    const QVector<ProjectedNode> second = projection.project(
        fixture.world, fixture.nodes, perspectiveView());
    QSet<NodeId> foregroundIds;
    QSet<NodeId> fogHintIds;
    QCOMPARE(first.size(), qsizetype(9));
    QCOMPARE(first.size(), second.size());
    for (qsizetype i = 0; i < first.size(); ++i)
        QCOMPARE(first.at(i).id, second.at(i).id);

    for (const ProjectedNode &node : first) {
        if (node.parentId == NodeId(1)) {
            if (node.fogHint) {
                fogHintIds.insert(node.id);
                QVERIFY(node.depthOpacity
                        <= ThemeRegistry::active()
                            .perspective.fogHintOpacity);
                QVERIFY(node.depthOpacity
                        >= ThemeRegistry::active()
                            .perspective.minimumHitOpacity);
            } else {
                foregroundIds.insert(node.id);
            }
            QVERIFY(!node.branchSurface.isEmpty());
            QVERIFY(!node.hitArea.isEmpty());
            continue;
        }
        QVERIFY(node.depthLayer != 2);
    }

    QCOMPARE(foregroundIds, QSet<NodeId>({2, 3, 4, 5, 6, 7}));
    QCOMPARE(fogHintIds, QSet<NodeId>({8, 9}));
    QVERIFY(!projectedNode(first, 10));
    for (NodeId descendantId = NodeId(directChildCount + 2);
         descendantId < NodeId(
             directChildCount + 2
             + directChildCount * descendantsPerChild);
         ++descendantId) {
        QVERIFY(!projectedNode(first, descendantId));
    }

    const QVector<FileNode> nonRootNodes = FakeTreeData::create();
    const WorldLayout nonRootWorld =
        WorldLayoutEngine().layout(nonRootNodes, 6);
    const QVector<ProjectedNode> nonRoot = projection.project(
        nonRootWorld, nonRootNodes, perspectiveView());
    QVERIFY(nonRoot.size() <= 10);
    QVERIFY(std::ranges::none_of(
        nonRoot, [](const ProjectedNode &node) {
            return node.depthLayer == 2;
        }));
    const ProjectedNode *parent = projectedNode(nonRoot, 1);
    QVERIFY(parent);
    QVERIFY(parent->parentContext);
}

void TreeLayoutEngineTest::perspectiveNonRootBudgetHasExactCeiling()
{
    constexpr NodeId parentId = 100;
    PerspectiveHierarchy fixture = perspectiveHierarchy(16, 2);
    fixture.nodes.first().parentId = parentId;
    fixture.nodes.append({
        .id = parentId,
        .parentId = InvalidNodeId,
        .name = QStringLiteral("Parent context"),
        .path = QStringLiteral("/parent"),
        .kind = NodeKind::Directory
    });
    fixture.world.nodes.first().parentId = parentId;
    fixture.world.nodes.prepend({
        .id = parentId,
        .parentId = InvalidNodeId,
        .role = WorldNodeRole::ParentContext,
        .semanticDepth = -1,
        .distanceAlongParent = -1.0
    });

    const QVector<ProjectedNode> projected =
        PerspectiveProjection().project(
            fixture.world, fixture.nodes, perspectiveView());
    qsizetype foregroundCount = 0;
    qsizetype fogHintCount = 0;
    qsizetype descendantCount = 0;
    for (const ProjectedNode &node : projected) {
        if (node.depthLayer == 2)
            ++descendantCount;
        if (node.parentId != NodeId(1))
            continue;
        if (node.fogHint)
            ++fogHintCount;
        else
            ++foregroundCount;
    }

    QCOMPARE(projected.size(), qsizetype(10));
    QCOMPARE(foregroundCount, qsizetype(6));
    QCOMPARE(fogHintCount, qsizetype(2));
    QCOMPARE(descendantCount, qsizetype(0));

    const ProjectedNode *parent = projectedNode(projected, parentId);
    const ProjectedNode *focus = projectedNode(projected, 1);
    QVERIFY(parent);
    QVERIFY(focus);
    QVERIFY(parent->parentContext);
    QVERIFY(!focus->parentContext);
    QVERIFY(!parent->fogHint);
    QVERIFY(!focus->fogHint);
}

void TreeLayoutEngineTest::perspectiveOverflowReflectsProjectionBudget()
{
    const PerspectiveProjection projection;

    QVector<FileNode> smallNodes = perspectiveNodes(5);
    smallNodes.first().totalChildCount = 5;
    const QVector<ProjectedNode> small = projection.project(
        perspectiveWorld(5), smallNodes, perspectiveView());
    const ProjectedNode *smallFocus = projectedNode(small, 1);
    QVERIFY(smallFocus);
    QCOMPARE(smallFocus->hiddenChildCount, quint64(0));

    QVector<FileNode> fullNodes = perspectiveNodes(16);
    fullNodes.first().totalChildCount = 16;
    const QVector<ProjectedNode> full = projection.project(
        perspectiveWorld(16), fullNodes, perspectiveView());
    const ProjectedNode *fullFocus = projectedNode(full, 1);
    QVERIFY(fullFocus);
    QCOMPARE(fullFocus->hiddenChildCount, quint64(8));

    QVector<FileNode> materializedNodes = perspectiveNodes(16);
    materializedNodes.first().totalChildCount = 0;
    const QVector<ProjectedNode> materialized = projection.project(
        perspectiveWorld(16), materializedNodes, perspectiveView());
    const ProjectedNode *materializedFocus =
        projectedNode(materialized, 1);
    QVERIFY(materializedFocus);
    QCOMPARE(materializedFocus->hiddenChildCount, quint64(8));

    QVector<FileNode> partialNodes = perspectiveNodes(16);
    partialNodes.first().totalChildCount = 1'000'000;
    partialNodes.first().directoryState = DirectoryLoadState::Partial;
    WorldLayout partialWorld = perspectiveWorld(16);
    partialWorld.nodes.first().hiddenChildCount = 1'000'000 - 16;
    const QVector<ProjectedNode> partial = projection.project(
        partialWorld, partialNodes, perspectiveView());
    const ProjectedNode *partialFocus = projectedNode(partial, 1);
    QVERIFY(partialFocus);
    QCOMPARE(partialFocus->hiddenChildCount, quint64(1'000'000 - 8));
}

void TreeLayoutEngineTest::perspectiveWindowSlidesAndRestores()
{
    const PerspectiveHierarchy fixture = perspectiveHierarchy(16, 0);
    const PerspectiveProjection projection;
    const QVector<ProjectedNode> initial = projection.project(
        fixture.world, fixture.nodes, perspectiveView());
    const QVector<ProjectedNode> advanced = projection.project(
        fixture.world, fixture.nodes, perspectiveView(2.1));
    const QVector<ProjectedNode> restored = projection.project(
        fixture.world, fixture.nodes, perspectiveView());
    QSet<NodeId> initialDirectIds;
    QSet<NodeId> advancedDirectIds;

    for (const ProjectedNode &node : initial) {
        if (node.parentId == NodeId(1))
            initialDirectIds.insert(node.id);
    }
    for (const ProjectedNode &node : advanced) {
        if (node.parentId == NodeId(1))
            advancedDirectIds.insert(node.id);
    }
    QCOMPARE(initialDirectIds,
             QSet<NodeId>({2, 3, 4, 5, 6, 7, 8, 9}));
    QCOMPARE(advancedDirectIds,
             QSet<NodeId>({3, 4, 5, 6, 7, 8, 9, 10}));

    QCOMPARE(restored.size(), initial.size());
    for (const ProjectedNode &initialNode : initial) {
        const ProjectedNode *restoredNode =
            projectedNode(restored, initialNode.id);
        QVERIFY(restoredNode);
        QCOMPARE(restoredNode->position, initialNode.position);
        QCOMPARE(restoredNode->cameraDepth, initialNode.cameraDepth);
        QCOMPARE(restoredNode->depthOpacity, initialNode.depthOpacity);
        QCOMPARE(restoredNode->fogHint, initialNode.fogHint);
        QCOMPARE(restoredNode->branchSurface,
                 initialNode.branchSurface);
        QCOMPARE(restoredNode->hitArea, initialNode.hitArea);
    }
}

void TreeLayoutEngineTest::perspectiveWindowUsesDeterministicOrdering()
{
    WorldLayout world {.focusedId = 1};
    world.nodes.append({
        .id = 1,
        .role = WorldNodeRole::Focus
    });
    world.nodes.append({
        .id = 2,
        .parentId = 1,
        .role = WorldNodeRole::DirectChild,
        .lane = 2,
        .distanceAlongParent = 1.0
    });
    world.nodes.append({
        .id = 4,
        .parentId = 1,
        .role = WorldNodeRole::DirectChild,
        .lane = 0,
        .distanceAlongParent = 1.0
    });
    world.nodes.append({
        .id = 3,
        .parentId = 1,
        .role = WorldNodeRole::DirectChild,
        .lane = 0,
        .distanceAlongParent = 1.0
    });

    const PerspectiveDirectChildWindow window =
        PerspectivePresentationPolicy::select(
            world, perspectiveView(),
            ThemeRegistry::active().perspective);
    QCOMPARE(window.foregroundIds, QVector<NodeId>({3, 4, 2}));
    QVERIFY(window.fogHintIds.isEmpty());
    QCOMPARE(window.materializedDirectChildCount, qsizetype(3));
}

void TreeLayoutEngineTest::perspectiveContentsPolicyUsesEightEntryBudget()
{
    const PerspectiveProfile &profile =
        ThemeRegistry::active().perspective;
    QVector<FileNode> smallNodes = perspectiveNodes(5);
    QVERIFY(!PerspectivePresentationPolicy::requiresContents(
        smallNodes.first(), profile));

    QVector<FileNode> fullNodes = perspectiveNodes(16);
    QVERIFY(PerspectivePresentationPolicy::requiresContents(
        fullNodes.first(), profile));
    QVERIFY(!DirectoryPresentationPolicy::evaluate(fullNodes.first())
                 .usesContentsFallback);

    QVector<FileNode> incompleteNodes = perspectiveNodes(5);
    incompleteNodes.first().directoryState =
        DirectoryLoadState::Partial;
    QVERIFY(PerspectivePresentationPolicy::requiresContents(
        incompleteNodes.first(), profile));
}

void TreeLayoutEngineTest::perspectiveProjectionUsesDepthOrder()
{
    const PerspectiveProjection projection;
    const QVector<ProjectedNode> projected = projection.project(
        perspectiveWorld(8), perspectiveNodes(8), perspectiveView());

    QVERIFY(projected.size() > 2);
    for (qsizetype i = 1; i < projected.size(); ++i) {
        QVERIFY2(projected.at(i - 1).cameraDepth
                     >= projected.at(i).cameraDepth,
                 "Paint order must run from far growth to near growth");
    }
    for (auto it = projected.crbegin() + 1;
         it != projected.crend(); ++it) {
        QVERIFY2((it - 1)->cameraDepth <= it->cameraDepth,
                 "Reverse iteration must hit near growth first");
    }
}

void TreeLayoutEngineTest::perspectiveProjectionTracksViewport()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(1);
    const WorldLayout world = perspectiveWorld(1);
    const QVector<ProjectedNode> compact = projection.project(
        world, nodes, {.viewport = {800.0, 600.0}});
    const QVector<ProjectedNode> wide = projection.project(
        world, nodes, {.viewport = {1400.0, 900.0}});
    const ProjectedNode *compactFocus = projectedNode(compact, 1);
    const ProjectedNode *wideFocus = projectedNode(wide, 1);
    QVERIFY(compactFocus);
    QVERIFY(wideFocus);

    QCOMPARE(compactFocus->position.x(), qreal(400.0));
    QCOMPARE(wideFocus->position.x(), qreal(700.0));
    QVERIFY(wideFocus->position.y() > compactFocus->position.y());
    QVERIFY(wideFocus->branchSurface.boundingRect().center().x()
            > compactFocus->branchSurface.boundingRect().center().x());
}

void TreeLayoutEngineTest::perspectiveProjectionBuildsTaperedSurfaces()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(1);
    const QVector<ProjectedNode> projected = projection.project(
        perspectiveWorld(1), nodes, perspectiveView());
    const ProjectedNode *focus = projectedNode(projected, 1);
    const ProjectedNode *child = projectedNode(projected, 2);
    QVERIFY(focus);
    QVERIFY(child);

    QVERIFY(!focus->branchSurface.isEmpty());
    QVERIFY(!child->branchSurface.isEmpty());
    QVERIFY(focus->branchStartWidth > focus->branchEndWidth);
    QVERIFY(child->branchStartWidth > child->branchEndWidth);
    QVERIFY(approximately(focus->branchStartWidth, 130.0));
    QVERIFY(approximately(focus->branchEndWidth, 68.3333));
    QVERIFY(approximately(child->branchStartWidth, 75.0));
    QVERIFY(approximately(child->branchEndWidth, 30.8571));
    QVERIFY(focus->branchStartWidth > child->branchStartWidth);
}

void TreeLayoutEngineTest::perspectiveHitGeometryContainsPaintedSurfaces()
{
    const PerspectiveProjection projection;
    const QVector<FileNode> nodes = perspectiveNodes(8);
    const QVector<ProjectedNode> projected = projection.project(
        perspectiveWorld(8), nodes, perspectiveView());

    for (const ProjectedNode &node : projected) {
        QVERIFY2(!node.branchSurface.isEmpty(),
                 "Every projected node needs painted surface geometry");
        QVERIFY2(!node.hitArea.isEmpty(),
                 "Every painted surface needs matching hit geometry");
        QVERIFY2(node.branchSurface.subtracted(node.hitArea).isEmpty(),
                 "Hit geometry must contain the complete painted surface");
        QVERIFY(node.hitArea.boundingRect().contains(
            node.branchSurface.boundingRect()));

        QPainterPath centerLine(node.branchStart);
        centerLine.cubicTo(
            node.branchControl1,
            node.branchControl2,
            node.position);
        const QPointF midpoint = centerLine.pointAtPercent(0.5);
        QVERIFY(node.branchSurface.contains(midpoint));
        QVERIFY(node.hitArea.contains(midpoint));
        QVERIFY(!node.hitArea.contains(
            midpoint + QPointF(500.0, 500.0)));
    }
}

void TreeLayoutEngineTest::layoutIsDeterministic()
{
    const QVector<FileNode> nodes = FakeTreeData::create();
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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
    TreeLayoutEngine engine(ProjectionKind::Planar);
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
    const TreeLayoutEngine engine(ProjectionKind::Planar);
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

    const TreeLayoutEngine engine(ProjectionKind::Planar);
    const QVector<LayoutNode> layout = engine.layout(nodes, 1);

    QCOMPARE(layout.size(), DirectoryPresentationPolicy::VisualChildLimit + 1);
    QCOMPARE(
        layout.first().hiddenChildCount,
        quint64(1'000'000 - DirectoryPresentationPolicy::VisualChildLimit));
}

QTEST_GUILESS_MAIN(TreeLayoutEngineTest)

#include "TreeLayoutEngineTest.moc"
