#include "FakeTreeData.h"
#include "TreeLayoutEngine.h"

#include <QSet>
#include <QtTest>

class TreeLayoutEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtureHasStableRelationships();
    void layoutIsDeterministic();
    void focusedDirectoryKeepsParentContext();
    void wideDirectoryUsesUniquePositions();
    void wideDirectoryReservesLabelSpace();
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

QTEST_GUILESS_MAIN(TreeLayoutEngineTest)

#include "TreeLayoutEngineTest.moc"
