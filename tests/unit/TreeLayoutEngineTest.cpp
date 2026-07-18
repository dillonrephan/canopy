#include "FakeTreeData.h"
#include "LabelVisibilityPolicy.h"
#include "NavigationState.h"
#include "TreeLayoutEngine.h"

#include <QSet>
#include <QtTest>

#include <algorithm>

class TreeLayoutEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureHasStableRelationships();
    void layoutIsDeterministic();
    void focusedDirectoryKeepsParentContext();
    void wideDirectoryUsesUniquePositions();
    void wideDirectoryReservesLabelSpace();
    void homeDirectChildrenReserveLabelSpace();
    void normalZoomPrioritizesHomeLevelLabels();
    void lowZoomKeepsFocusAndEmphasizedLabels();
    void closeZoomRevealsOnlyNonOverlappingDescendants();
    void navigationStateTracksSelectionAndHistory();
    void layoutProvidesConnectedBranchGeometry();
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
        QCOMPARE(node.branchStart, parent->position);
    }
}

QTEST_GUILESS_MAIN(TreeLayoutEngineTest)

#include "TreeLayoutEngineTest.moc"
