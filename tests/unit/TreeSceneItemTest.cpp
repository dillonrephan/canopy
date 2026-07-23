#include "TreeSceneItem.h"
#include "ThemeRegistry.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>

#include <algorithm>

class TreeSceneItemTest final : public QObject
{
    Q_OBJECT

private slots:
    void reducedMotionTravelSnapsWithOneSignal();
    void perspectiveWheelEventsDriveBoundedTravel();
    void perspectiveBlankDoubleClickAndReset();
    void focusAndUpResetTravelWithOneSignal();
    void liveRemovalClampsRangeAndDepth();
    void emptyDirectoryTravelIsSignalFree();
    void offscreenPaintTracksTravelAndResetState();
    void perspectiveFocusTransitionSharesPaintAndHitGeometry();
    void planarReducedMotionSnapsCameraWithOneSignal();
    void removedDepartureSnapshotPaintsWithoutHitTarget();
    void removedHistoryEntryIsReconciledBeforeNavigationSignal();
};

namespace {
class EventTreeSceneItem final : public TreeSceneItem
{
public:
    using TreeSceneItem::TreeSceneItem;

    bool doubleClickBlank(const QPointF &position)
    {
        QMouseEvent event(
            QEvent::MouseButtonDblClick,
            position,
            position,
            position,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        event.ignore();
        mouseDoubleClickEvent(&event);
        return event.isAccepted();
    }

    bool sendWheel(
        const QPoint &angleDelta,
        const QPoint &pixelDelta = {},
        Qt::ScrollPhase phase = Qt::NoScrollPhase,
        bool inverted = false)
    {
        QWheelEvent event(
            QPointF(500.0, 400.0),
            QPointF(500.0, 400.0),
            pixelDelta,
            angleDelta,
            Qt::NoButton,
            Qt::NoModifier,
            phase,
            inverted);
        event.ignore();
        wheelEvent(&event);
        return event.isAccepted();
    }

    void clickAt(const QPointF &position)
    {
        QMouseEvent press(
            QEvent::MouseButtonPress,
            position,
            position,
            position,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        mousePressEvent(&press);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            position,
            position,
            position,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        mouseReleaseEvent(&release);
    }
};

void makeDirectory(const QDir &root, const QString &name)
{
    QVERIFY2(root.mkdir(name), qPrintable(root.filePath(name)));
}

void makeFile(const QString &path)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write("leaf"), qint64(4));
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


QPoint firstChangedPixel(const QImage &left, const QImage &right)
{
    Q_ASSERT(left.size() == right.size());
    for (int y = 0; y < left.height(); ++y) {
        const auto *leftLine = reinterpret_cast<const QRgb *>(
            left.constScanLine(y));
        const auto *rightLine = reinterpret_cast<const QRgb *>(
            right.constScanLine(y));
        for (int x = 0; x < left.width(); ++x) {
            if (leftLine[x] != rightLine[x])
                return {x, y};
        }
    }
    return {-1, -1};
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

void TreeSceneItemTest::perspectiveWheelEventsDriveBoundedTravel()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    for (int index = 0; index < 12; ++index) {
        makeDirectory(
            root,
            QStringLiteral("Branch %1")
                .arg(index, 2, 10, QLatin1Char('0')));
    }

    EventTreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(12), 5000);
    QVERIFY(scene.maximumTravelDepth() >= 1.0);

    QVERIFY(scene.sendWheel({0, 120}));
    QCOMPARE(scene.m_travelTargetDepth, qreal(0.5));
    QVERIFY(scene.sendWheel({0, 120}));
    QCOMPARE(scene.m_travelTargetDepth, qreal(1.0));
    scene.setReducedMotion(true);
    QCOMPARE(scene.travelDepth(), qreal(1.0));

    scene.resetView();
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    scene.setReducedMotion(false);
    const PerspectiveProfile &perspective =
        ThemeRegistry::active().perspective;
    const qreal touchpadPixelsPerStep =
        perspective.touchpadPixelsPerStep;
    QVERIFY(touchpadPixelsPerStep > 2.0);
    const int partialPixelDelta = qMax(
        1, qFloor(touchpadPixelsPerStep / 2.0));
    QVERIFY(partialPixelDelta < touchpadPixelsPerStep);
    const qreal firstTouchpadTarget =
        partialPixelDelta / touchpadPixelsPerStep
        * perspective.wheelTravelPerStep;
    QVERIFY(scene.sendWheel(
        {}, {0, partialPixelDelta}, Qt::ScrollUpdate));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(scene.m_travelStartDepth, qreal(0.0));
    QCOMPARE(scene.m_travelTargetDepth, firstTouchpadTarget);

    const qreal accumulatedTouchpadTarget =
        firstTouchpadTarget
        + partialPixelDelta / touchpadPixelsPerStep
            * perspective.wheelTravelPerStep;
    QVERIFY(scene.sendWheel(
        {}, {0, partialPixelDelta}, Qt::ScrollUpdate));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(scene.m_travelStartDepth, qreal(0.0));
    QCOMPARE(scene.m_travelTargetDepth, accumulatedTouchpadTarget);
    scene.setReducedMotion(true);
    QCOMPARE(scene.travelDepth(), accumulatedTouchpadTarget);

    scene.resetView();
    scene.setReducedMotion(false);
    QVERIFY(scene.sendWheel({0, 240}));
    const int duration = ThemeRegistry::active().motion.cameraDuration;
    QVERIFY(duration > 1);
    scene.m_travelAnimation.setCurrentTime(duration / 2);
    const qreal reversalStart = scene.travelDepth();
    QVERIFY(reversalStart > 0.0);
    QVERIFY(scene.sendWheel({0, -120}));
    const qreal expectedReversal = qMax(
        qreal(0.0), reversalStart - qreal(0.5));
    QCOMPARE(scene.m_travelStartDepth, reversalStart);
    QCOMPARE(scene.m_travelTargetDepth, expectedReversal);
    scene.setReducedMotion(true);
    QCOMPARE(scene.travelDepth(), expectedReversal);

    scene.resetView();
    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    QVERIFY(scene.sendWheel({0, -120}));
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QCOMPARE(cameraSpy.count(), 0);

    travelToEnd(scene);
    cameraSpy.clear();
    const qreal maximum = scene.maximumTravelDepth();
    QVERIFY(scene.sendWheel({0, 120}));
    QCOMPARE(scene.travelDepth(), maximum);
    QCOMPARE(cameraSpy.count(), 0);
}

void TreeSceneItemTest::perspectiveBlankDoubleClickAndReset()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    for (int index = 0; index < 12; ++index) {
        makeDirectory(
            root,
            QStringLiteral("Branch %1")
                .arg(index, 2, 10, QLatin1Char('0')));
    }

    EventTreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    scene.setReducedMotion(true);
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(12), 5000);
    QVERIFY(scene.maximumTravelDepth() > 0.0);

    const QPointF blankPoint(999.0, 1.0);
    QCOMPARE(scene.nodeAt(blankPoint), InvalidNodeId);
    QVERIFY(scene.doubleClickBlank(blankPoint));
    QCOMPARE(scene.travelDepth(), scene.maximumTravelDepth());
    QVERIFY(scene.canResetView());

    QSignalSpy cameraSpy(&scene, &TreeSceneItem::cameraChanged);
    scene.resetView();
    QCOMPARE(scene.travelDepth(), qreal(0.0));
    QVERIFY(!scene.canResetView());
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

    QTRY_VERIFY(scene.maximumTravelDepth() > 0.0);
    scene.travelForward();
    QVERIFY(scene.travelDepth() > 0.0);
    cameraSpy.clear();

    scene.goBack();
    QCOMPARE(scene.breadcrumb(), branch.path());
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

void TreeSceneItemTest::perspectiveFocusTransitionSharesPaintAndHitGeometry()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    makeDirectory(root, QStringLiteral("Branch"));
    makeDirectory(root, QStringLiteral("Sibling"));
    const QDir branch(root.filePath(QStringLiteral("Branch")));
    makeDirectory(branch, QStringLiteral("Nested A"));
    makeDirectory(branch, QStringLiteral("Nested B"));

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(2), 5000);

    const auto branchNode = std::find_if(
        scene.m_filesystemModel.nodes().cbegin(),
        scene.m_filesystemModel.nodes().cend(),
        [&branch](const FileNode &node) {
            return node.path == branch.path();
        });
    QVERIFY(branchNode != scene.m_filesystemModel.nodes().cend());
    const NodeId branchId = branchNode->id;

    QVERIFY(scene.navigateToPath(branch.path()));
    const int duration = ThemeRegistry::active().motion.focusDuration;
    QVERIFY(duration > 1);
    scene.m_focusAnimation.setCurrentTime(0);
    QVERIFY(scene.m_previousProjectedLayout.contains(branchId));

    const LayoutNode initial =
        scene.m_previousProjectedLayout.value(branchId);
    const auto finalIt = std::find_if(
        scene.m_layout.cbegin(), scene.m_layout.cend(),
        [branchId](const LayoutNode &layout) {
            return layout.id == branchId;
        });
    QVERIFY(finalIt != scene.m_layout.cend());
    const LayoutNode final = *finalIt;

    scene.m_focusAnimation.setCurrentTime(duration / 2);
    const QVector<LayoutNode> middleLayout =
        scene.resolvedPerspectiveLayout();
    const auto middleIt = std::find_if(
        middleLayout.cbegin(), middleLayout.cend(),
        [branchId](const LayoutNode &layout) {
            return layout.id == branchId;
        });
    QVERIFY(middleIt != middleLayout.cend());
    const LayoutNode middle = *middleIt;
    QVERIFY(middle.position != initial.position);
    QVERIFY(middle.position != final.position);

    QPoint hitPixel(-1, -1);
    const QRect searchBounds = middle.branchSurface.boundingRect()
        .intersected(scene.boundingRect()).toAlignedRect();
    for (int y = searchBounds.top();
         y <= searchBounds.bottom() && hitPixel.x() < 0; ++y) {
        for (int x = searchBounds.left(); x <= searchBounds.right(); ++x) {
            const QPointF point(x + 0.5, y + 0.5);
            if (!middle.branchSurface.contains(point)
                || initial.hitArea.contains(point)
                || final.hitArea.contains(point)
                || middle.labelBounds.contains(point)
                || QLineF(point, middle.position).length()
                    <= middle.radius + 9.0
                || scene.nodeAt(point) != branchId) {
                continue;
            }
            hitPixel = {x, y};
            break;
        }
    }
    QVERIFY2(hitPixel.x() >= 0,
             "No branch-only midpoint unique to interpolated geometry");
    const QPointF hitPoint(hitPixel.x() + 0.5, hitPixel.y() + 0.5);
    QVERIFY(middle.hitArea.contains(hitPoint));
    QVERIFY(!initial.hitArea.contains(hitPoint));
    QVERIFY(!final.hitArea.contains(hitPoint));
    QCOMPARE(scene.nodeAt(hitPoint), branchId);

    const QImage middleFrame = renderScene(scene);
    QVERIFY(middleFrame.pixelColor(hitPixel)
            != ThemeRegistry::active().palette.sceneCanvas);

    scene.m_focusAnimation.setCurrentTime(0);
    QVERIFY(scene.nodeAt(hitPoint) != branchId);
    scene.m_focusAnimation.setCurrentTime(duration / 2);
    const QVector<LayoutNode> beforeInterruption =
        scene.resolvedPerspectiveLayout();
    const auto visibleIt = std::find_if(
        beforeInterruption.cbegin(), beforeInterruption.cend(),
        [branchId](const LayoutNode &layout) {
            return layout.id == branchId;
        });
    QVERIFY(visibleIt != beforeInterruption.cend());
    const LayoutNode visibleBeforeInterruption = *visibleIt;

    QVERIFY(scene.navigateToPath(root.path()));
    scene.m_focusAnimation.setCurrentTime(0);
    QVERIFY(scene.m_previousProjectedLayout.contains(branchId));
    QCOMPARE(scene.m_previousProjectedLayout.value(branchId).position,
             visibleBeforeInterruption.position);
    QCOMPARE(scene.m_previousProjectedLayout.value(branchId).branchSurface,
             visibleBeforeInterruption.branchSurface);
    const QVector<LayoutNode> interruptedStart =
        scene.resolvedPerspectiveLayout();
    const auto interruptedIt = std::find_if(
        interruptedStart.cbegin(), interruptedStart.cend(),
        [branchId](const LayoutNode &layout) {
            return layout.id == branchId;
        });
    QVERIFY(interruptedIt != interruptedStart.cend());
    QCOMPARE(interruptedIt->branchSurface,
             visibleBeforeInterruption.branchSurface);

    scene.m_focusAnimation.setCurrentTime(duration);
    QVERIFY(scene.nodeAt(hitPoint) != branchId);
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

void TreeSceneItemTest::removedDepartureSnapshotPaintsWithoutHitTarget()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString leafPath = temporaryDirectory.filePath(
        QStringLiteral("departing.txt"));
    makeFile(leafPath);

    EventTreeSceneItem scene(
        temporaryDirectory.path(), ProjectionKind::Planar);
    scene.setSize({1000.0, 800.0});
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(1), 3000);
    const QString rootPath = scene.selectedPath();

    QImage departureFrame;
    connect(&scene, &TreeSceneItem::selectionChanged,
            this, [&scene, &departureFrame] {
                if (scene.selectedMaterializedChildCount() == 0)
                    departureFrame = renderScene(scene);
            });

    QVERIFY(QFile::remove(leafPath));
    QTRY_VERIFY_WITH_TIMEOUT(!departureFrame.isNull(), 5000);
    QCOMPARE(scene.selectedMaterializedChildCount(), qulonglong(0));

    EventTreeSceneItem clearedScene(
        temporaryDirectory.path(), ProjectionKind::Planar);
    clearedScene.setSize({1000.0, 800.0});
    clearedScene.setReducedMotion(true);
    QTRY_COMPARE_WITH_TIMEOUT(
        clearedScene.selectedMaterializedChildCount(), qulonglong(0), 3000);
    const QImage clearedFrame = renderScene(clearedScene);
    QVERIFY(changedPixelCount(departureFrame, clearedFrame) > 100);

    const QPoint paintedDeparture =
        firstChangedPixel(departureFrame, clearedFrame);
    QVERIFY(paintedDeparture.x() >= 0);
    scene.clickAt(paintedDeparture);
    QCOMPARE(scene.selectedPath(), rootPath);

    scene.setReducedMotion(true);
    QCOMPARE(renderScene(scene), clearedFrame);
}

void TreeSceneItemTest::removedHistoryEntryIsReconciledBeforeNavigationSignal()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    QDir root(temporaryDirectory.path());
    QVERIFY(root.mkdir(QStringLiteral("branch")));
    const QString branchPath = root.filePath(QStringLiteral("branch"));

    TreeSceneItem scene(temporaryDirectory.path());
    scene.setSize({1000.0, 800.0});
    scene.setReducedMotion(true);
    QTRY_COMPARE_WITH_TIMEOUT(
        scene.selectedMaterializedChildCount(), qulonglong(1), 3000);
    QVERIFY(scene.navigateToPath(branchPath));
    scene.goUp();
    QCOMPARE(scene.breadcrumb(), root.path());
    QVERIFY(scene.canGoBack());

    QSignalSpy navigationSpy(&scene, &TreeSceneItem::navigationChanged);
    QVERIFY(QDir(branchPath).removeRecursively());
    makeFile(branchPath);
    QTRY_VERIFY_WITH_TIMEOUT(!scene.canGoBack(), 5000);
    QVERIFY(navigationSpy.count() >= 1);
    QCOMPARE(scene.breadcrumb(), root.path());
    QCOMPARE(scene.selectedMaterializedChildCount(), qulonglong(1));

    scene.goBack();
    QCOMPARE(scene.breadcrumb(), root.path());
}

QTEST_MAIN(TreeSceneItemTest)

#include "TreeSceneItemTest.moc"
