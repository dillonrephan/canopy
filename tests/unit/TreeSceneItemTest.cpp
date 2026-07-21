#include "TreeSceneItem.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TreeSceneItemTest final : public QObject
{
    Q_OBJECT

private slots:
    void reducedMotionTravelSnapsWithOneSignal();
    void focusAndUpResetTravelWithOneSignal();
    void liveRemovalClampsRangeAndDepth();
    void emptyDirectoryTravelIsSignalFree();
};

namespace {
void makeDirectory(const QDir &root, const QString &name)
{
    QVERIFY2(root.mkdir(name), qPrintable(root.filePath(name)));
}

void travelToEnd(TreeSceneItem &scene)
{
    for (int step = 0; step < 64 && scene.canTravelForward(); ++step)
        scene.travelForward();
    QVERIFY(!scene.canTravelForward());
    QCOMPARE(scene.travelDepth(), scene.maximumTravelDepth());
}
}

void TreeSceneItemTest::reducedMotionTravelSnapsWithOneSignal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    makeDirectory(root, QStringLiteral("Branch A"));
    makeDirectory(root, QStringLiteral("Branch B"));
    makeDirectory(root, QStringLiteral("Branch C"));

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    QTRY_VERIFY(scene.maximumTravelDepth() > 0.0);
    scene.setReducedMotion(true);

    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    scene.travelForward();

    QCOMPARE(scene.travelDepth(), qreal(0.5));
    QCOMPARE(cameraSpy.count(), 1);
}

void TreeSceneItemTest::focusAndUpResetTravelWithOneSignal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    makeDirectory(root, QStringLiteral("Branch"));
    makeDirectory(root, QStringLiteral("Sibling"));
    QDir branch(root.filePath(QStringLiteral("Branch")));
    makeDirectory(branch, QStringLiteral("Nested A"));
    makeDirectory(branch, QStringLiteral("Nested B"));

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    scene.setReducedMotion(true);
    QTRY_VERIFY(scene.maximumTravelDepth() > 0.0);
    scene.travelForward();
    QVERIFY(scene.travelDepth() > 0.0);

    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    QVERIFY(scene.navigateToPath(branch.path()));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(cameraSpy.count(), 1);

    QTRY_VERIFY(scene.maximumTravelDepth() > 0.0);
    scene.travelForward();
    QVERIFY(scene.travelDepth() > 0.0);
    cameraSpy.clear();

    scene.goUp();
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(cameraSpy.count(), 1);
}

void TreeSceneItemTest::liveRemovalClampsRangeAndDepth()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    QStringList childNames;
    for (int index = 0; index < 8; ++index) {
        const QString name = QStringLiteral("Branch %1").arg(index);
        childNames.append(name);
        makeDirectory(root, name);
    }

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    scene.setReducedMotion(true);
    QTRY_VERIFY(scene.maximumTravelDepth() > 0.0);
    travelToEnd(scene);
    QVERIFY(scene.travelDepth() > 0.0);

    for (const QString &name : childNames)
        QVERIFY2(root.rmdir(name), qPrintable(root.filePath(name)));

    QTRY_COMPARE(scene.maximumTravelDepth(), qreal(0.0));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QVERIFY(!scene.canTravelForward());
    QVERIFY(!scene.canTravelBackward());
}

void TreeSceneItemTest::emptyDirectoryTravelIsSignalFree()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    QCOMPARE(scene.maximumTravelDepth(), qreal(0.0));
    QCOMPARE(scene.travelDepth(), qreal(0.0));

    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    scene.travelForward();
    scene.travelBackward();
    scene.resetView();

    QCOMPARE(scene.maximumTravelDepth(), qreal(0.0));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(cameraSpy.count(), 0);
}

QTEST_MAIN(TreeSceneItemTest)

#include "TreeSceneItemTest.moc"
