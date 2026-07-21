#include "TreeSceneItem.h"
#include "ThemeRegistry.h"
#include "ThemeTokens.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>


class TreeSceneItemTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reducedMotionTravelSnapsWithOneSignal();
    void focusAndUpResetTravelWithOneSignal();
    void liveRemovalClampsRangeAndDepth();
    void emptyDirectoryTravelIsSignalFree();
    void offscreenPaintTracksTravelAndResetState();
    void planarReducedMotionSnapsCameraWithOneSignal();
    void removedDepartureSnapshotPaintsWithoutHitTarget();
    void removedHistoryEntryIsReconciledBeforeNavigationSignal();
    void qmlTravelControlsAndShortcutHandlersDriveScene();
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

void clickItem(QQuickWindow *window, QQuickItem *item)
{
    QVERIFY(item->isVisible());
    QVERIFY(item->property("enabled").toBool());
    const QPointF center = item->mapToScene(item->boundingRect().center());
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
}
}

void TreeSceneItemTest::initTestCase()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<TreeSceneItem>("Canopy", 1, 0, "TreeScene");
    qmlRegisterSingletonType<ThemeTokens>(
        "Canopy", 1, 0, "Theme",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ThemeTokens;
        });
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

void TreeSceneItemTest::qmlTravelControlsAndShortcutHandlersDriveScene()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QDir root(temporaryDirectory.path());
    for (int index = 0; index < 12; ++index)
        makeDirectory(root, QStringLiteral("Branch %1").arg(index));

    QQmlEngine engine;
    const QUrl mainQml = QUrl::fromLocalFile(
        QStringLiteral(CANOPY_SOURCE_DIR "/qml/Main.qml"));
    QQmlComponent component(&engine, mainQml);
    QScopedPointer<QObject> rootObject(component.createWithInitialProperties({
        {QStringLiteral("initialPath"), temporaryDirectory.path()},
    }));
    QVERIFY2(rootObject, qPrintable(component.errorString()));

    auto *window = qobject_cast<QQuickWindow *>(rootObject.get());
    QVERIFY(window);
    auto *tree = window->findChild<TreeSceneItem *>(QStringLiteral("treeScene"));
    auto *backButton = window->findChild<QQuickItem *>(
        QStringLiteral("travelBackButton"));
    auto *forwardButton = window->findChild<QQuickItem *>(
        QStringLiteral("travelForwardButton"));
    auto *resetButton = window->findChild<QQuickItem *>(
        QStringLiteral("resetViewButton"));
    auto *backShortcut = window->findChild<QObject *>(
        QStringLiteral("travelBackShortcut"));
    auto *forwardShortcut = window->findChild<QObject *>(
        QStringLiteral("travelForwardShortcut"));
    auto *resetShortcut = window->findChild<QObject *>(
        QStringLiteral("resetViewShortcut"));
    QVERIFY(tree);
    QVERIFY(backButton);
    QVERIFY(forwardButton);
    QVERIFY(resetButton);
    QVERIFY(backShortcut);
    QVERIFY(forwardShortcut);
    QVERIFY(resetShortcut);

    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_COMPARE(tree->breadcrumb(), temporaryDirectory.path());
    QTRY_COMPARE(tree->selectedMaterializedChildCount(), qulonglong(12));
    QTRY_VERIFY(tree->maximumTravelDepth() >= 1.0);
    tree->setReducedMotion(true);

    clickItem(window, forwardButton);
    QCOMPARE(tree->travelDepth(), qreal(0.5));
    clickItem(window, backButton);
    QCOMPARE(tree->travelDepth(), qreal(0.0));

    clickItem(window, forwardButton);
    QCOMPARE(tree->travelDepth(), qreal(0.5));
    clickItem(window, resetButton);
    QCOMPARE(tree->travelDepth(), qreal(0.0));

    tree->forceActiveFocus();
    QVERIFY(tree->hasActiveFocus());
    QCOMPARE(
        forwardShortcut->property("sequence").toString(),
        QStringLiteral("PageDown"));
    QCOMPARE(
        backShortcut->property("sequence").toString(),
        QStringLiteral("PageUp"));
    QCOMPARE(
        resetShortcut->property("sequence").toString(),
        QStringLiteral("Ctrl+0"));

    QVERIFY(forwardShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(forwardShortcut, "activated"));
    QCOMPARE(tree->travelDepth(), qreal(0.5));
    QVERIFY(backShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(backShortcut, "activated"));
    QCOMPARE(tree->travelDepth(), qreal(0.0));

    QVERIFY(QMetaObject::invokeMethod(forwardShortcut, "activated"));
    QCOMPARE(tree->travelDepth(), qreal(0.5));
    QVERIFY(resetShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(resetShortcut, "activated"));
    QCOMPARE(tree->travelDepth(), qreal(0.0));
}

QTEST_MAIN(TreeSceneItemTest)

#include "TreeSceneItemTest.moc"
