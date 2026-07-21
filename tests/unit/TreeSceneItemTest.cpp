#include "TreeSceneItem.h"
#include "ThemeRegistry.h"

#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
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
    void offscreenPaintTracksTravelAndResetState();
    void planarReducedMotionSnapsCameraWithOneSignal();
};

namespace {
class EventTreeSceneItem final : public TreeSceneItem
{
public:
    using TreeSceneItem::TreeSceneItem;

    void doubleClickBlank(const QPointF &position)
    {
        QMouseEvent event(
            QEvent::MouseButtonDblClick,
            position,
            position,
            position,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        mouseDoubleClickEvent(&event);
    }
};

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

QImage renderScene(TreeSceneItem &scene)
{
    QImage image(1000, 800, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scene.paint(&painter);
    painter.end();
    return image;
}

qsizetype changedPixelCount(const QImage &left, const QImage &right)
{
    Q_ASSERT(left.size() == right.size());
    Q_ASSERT(left.format() == QImage::Format_ARGB32_Premultiplied);
    Q_ASSERT(right.format() == left.format());
    qsizetype changed = 0;
    for (int y = 0; y < left.height(); ++y) {
        const auto *leftLine = reinterpret_cast<const QRgb *>(
            left.constScanLine(y));
        const auto *rightLine = reinterpret_cast<const QRgb *>(
            right.constScanLine(y));
        for (int x = 0; x < left.width(); ++x)
            changed += leftLine[x] != rightLine[x];
    }
    return changed;
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

void TreeSceneItemTest::offscreenPaintTracksTravelAndResetState()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    for (int index = 0; index < 12; ++index) {
        const QString name = QStringLiteral("Branch %1")
            .arg(index, 2, 10, QLatin1Char('0'));
        makeDirectory(root, name);
        const QDir branch(root.filePath(name));
        makeDirectory(branch, QStringLiteral("Nested A"));
        makeDirectory(branch, QStringLiteral("Nested B"));
    }

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    scene.setReducedMotion(true);
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(12), 5000);
    QVERIFY(scene.maximumTravelDepth() >= 1.0);

    const QImage baseline = renderScene(scene);
    scene.travelForward();
    QCOMPARE(scene.travelDepth(), qreal(0.5));
    const QImage traveled = renderScene(scene);
    QVERIFY(changedPixelCount(baseline, traveled) > 1000);

    scene.travelBackward();
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(renderScene(scene), baseline);

    scene.travelForward();
    QVERIFY(scene.navigateToPath(
        root.filePath(QStringLiteral("Branch 00"))));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(2), 5000);
    const QImage focusReset = renderScene(scene);
    QVERIFY(changedPixelCount(baseline, focusReset) > 1000);

    scene.travelForward();
    QCOMPARE(scene.travelDepth(), qreal(0.5));
    const QImage directReducedMotionSnap = renderScene(scene);
    scene.travelBackward();
    QCOMPARE(scene.travelDepth(), qreal(0.0));

    scene.setReducedMotion(false);
    scene.travelForward();
    scene.setReducedMotion(true);
    QCOMPARE(scene.travelDepth(), qreal(0.5));
    QCOMPARE(renderScene(scene), directReducedMotionSnap);
}

void TreeSceneItemTest::planarReducedMotionSnapsCameraWithOneSignal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    EventTreeSceneItem scene(
        temporaryDirectory.path(), ProjectionKind::Planar);
    scene.setSize({1000.0, 800.0});
    QVERIFY(!scene.usesPerspectiveProjection());

    const QPointF clickPosition(990.0, 790.0);
    const QPointF initialPan(120.0, 35.0);
    const CameraProfile &camera = ThemeRegistry::active().camera;
    const qreal targetZoom = qBound(
        camera.minimumZoom,
        camera.blankDoubleClickFactor,
        camera.maximumZoom);
    const QPointF anchor = clickPosition - initialPan;
    const QPointF targetPan =
        scene.boundingRect().center() - anchor * targetZoom;

    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    scene.doubleClickBlank(clickPosition);
    cameraSpy.clear();

    scene.setReducedMotion(true);
    QCOMPARE(scene.zoom(), targetZoom);
    QCOMPARE(scene.pan(), targetPan);
    QCOMPARE(cameraSpy.count(), 1);

    cameraSpy.clear();
    scene.setReducedMotion(false);
    scene.setReducedMotion(true);
    QCOMPARE(cameraSpy.count(), 0);
}

QTEST_MAIN(TreeSceneItemTest)

#include "TreeSceneItemTest.moc"
