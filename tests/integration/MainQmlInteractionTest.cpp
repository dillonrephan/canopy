#include "ThemeTokens.h"
#include "TreeSceneItem.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJSEngine>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtQml/qqml.h>
#include <QtTest>

#include <memory>

class MainQmlInteractionTest final : public QObject
{
    Q_OBJECT

private slots:
    void shortcutsAndVisibleControlsDriveTravel();
    void visibleUpAndBackControlsRestoreNavigation();
};

namespace {
struct MainFixture {
    QTemporaryDir directory;
    QString branchPath;

    MainFixture()
        : directory(
            QString::fromLocal8Bit(qgetenv("HOME"))
                + QStringLiteral("/canopy-main-test-XXXXXX"))
    {
        if (!directory.isValid())
            return;
        const QDir root(directory.path());
        for (int index = 0; index < 12; ++index) {
            const QString name = QStringLiteral("Branch %1")
                .arg(index, 2, 10, QLatin1Char('0'));
            if (!root.mkdir(name)) {
                directory.remove();
                return;
            }
        }
        branchPath = root.filePath(QStringLiteral("Branch 00"));
        const QDir branch(branchPath);
        if (!branch.mkdir(QStringLiteral("Nested A"))
            || !branch.mkdir(QStringLiteral("Nested B"))) {
            directory.remove();
        }
    }

    [[nodiscard]] bool isValid() const
    {
        return directory.isValid() && !branchPath.isEmpty();
    }
};

struct LoadedMain {
    QList<QQmlError> warnings;
    QQmlEngine engine;
    std::unique_ptr<QObject> root;

    explicit LoadedMain(const QString &initialPath)
    {
        QObject::connect(
            &engine, &QQmlEngine::warnings,
            [&warnings = warnings](const QList<QQmlError> &reported) {
                warnings.append(reported);
            });
        QQmlComponent component(
            &engine, QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (!component.isReady()) {
            warnings.append(component.errors());
            return;
        }
        root.reset(component.createWithInitialProperties({
            {QStringLiteral("initialPath"), initialPath}
        }));
        if (!root)
            warnings.append(component.errors());
    }
};

QString warningText(const QList<QQmlError> &warnings)
{
    QStringList lines;
    lines.reserve(warnings.size());
    for (const QQmlError &warning : warnings)
        lines.append(warning.toString());
    return lines.join(QLatin1Char('\n'));
}

template<typename Type = QObject>
Type *requiredObject(QObject *root, const char *name)
{
    Type *result = root->findChild<Type *>(
        QString::fromLatin1(name));
    if (!result)
        qWarning("Missing QML object: %s", name);
    return result;
}

void clickItem(QQuickWindow *window, QQuickItem *item)
{
    Q_ASSERT(window);
    Q_ASSERT(item);
    const QPointF center = item->mapToScene(
        QPointF(item->width() * 0.5, item->height() * 0.5));
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
}

qreal realProperty(QObject *object, const char *name)
{
    return object->property(name).toReal();
}
}

void MainQmlInteractionTest::shortcutsAndVisibleControlsDriveTravel()
{
    MainFixture fixture;
    QVERIFY(fixture.isValid());
    LoadedMain loaded(fixture.directory.path());
    QVERIFY2(loaded.root, qPrintable(warningText(loaded.warnings)));
    QVERIFY2(loaded.warnings.isEmpty(),
             qPrintable(warningText(loaded.warnings)));

    auto *window = qobject_cast<QQuickWindow *>(loaded.root.get());
    QVERIFY(window);
    QTRY_VERIFY(window->isExposed());
    auto *tree = requiredObject<QQuickItem>(window, "treeScene");
    auto *address = requiredObject<QQuickItem>(window, "addressField");
    auto *reducedMotion = requiredObject<QQuickItem>(
        window, "reducedMotionCheckBox");
    auto *travelBack = requiredObject<QQuickItem>(
        window, "travelBackButton");
    auto *travelForward = requiredObject<QQuickItem>(
        window, "travelForwardButton");
    auto *reset = requiredObject<QQuickItem>(window, "resetButton");
    auto *pageDownShortcut = requiredObject(
        window, "pageDownShortcut");
    auto *pageUpShortcut = requiredObject(window, "pageUpShortcut");
    auto *resetShortcut = requiredObject(window, "resetShortcut");
    QVERIFY(tree);
    QVERIFY(address);
    QVERIFY(reducedMotion);
    QVERIFY(travelBack);
    QVERIFY(travelForward);
    QVERIFY(reset);
    QVERIFY(pageDownShortcut);
    QVERIFY(pageUpShortcut);
    QVERIFY(resetShortcut);

    QTRY_COMPARE(tree->property("breadcrumb").toString(),
                 fixture.directory.path());
    QTRY_COMPARE_WITH_TIMEOUT(
        tree->property("selectedMaterializedChildCount").toULongLong(),
        qulonglong(12), 5000);
    QTRY_VERIFY(realProperty(tree, "maximumTravelDepth") >= 1.0);

    clickItem(window, reducedMotion);
    QTRY_VERIFY(reducedMotion->property("checked").toBool());
    QTRY_VERIFY(tree->property("reducedMotion").toBool());

    tree->forceActiveFocus();
    QTRY_VERIFY(tree->hasActiveFocus());
    QTRY_VERIFY(pageDownShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(pageDownShortcut, "activated"));
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.5));
    QTRY_VERIFY(pageUpShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(pageUpShortcut, "activated"));
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));

    address->forceActiveFocus();
    QTRY_VERIFY(address->hasActiveFocus());
    QVERIFY(!pageDownShortcut->property("enabled").toBool());
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));

    QTRY_VERIFY(travelForward->property("enabled").toBool());
    QVERIFY(!travelBack->property("enabled").toBool());
    clickItem(window, travelForward);
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.5));
    QTRY_VERIFY(travelBack->property("enabled").toBool());
    clickItem(window, travelBack);
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));

    clickItem(window, travelForward);
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.5));
    QTRY_VERIFY(reset->property("enabled").toBool());
    clickItem(window, reset);
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));
    QVERIFY(!reset->property("enabled").toBool());

    tree->forceActiveFocus();
    QTRY_VERIFY(pageDownShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(pageDownShortcut, "activated"));
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.5));
    QVERIFY(resetShortcut->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(resetShortcut, "activated"));
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));
    QVERIFY2(loaded.warnings.isEmpty(),
             qPrintable(warningText(loaded.warnings)));
}

void MainQmlInteractionTest::visibleUpAndBackControlsRestoreNavigation()
{
    MainFixture fixture;
    QVERIFY(fixture.isValid());
    LoadedMain loaded(fixture.directory.path());
    QVERIFY2(loaded.root, qPrintable(warningText(loaded.warnings)));
    QVERIFY2(loaded.warnings.isEmpty(),
             qPrintable(warningText(loaded.warnings)));

    auto *window = qobject_cast<QQuickWindow *>(loaded.root.get());
    QVERIFY(window);
    QTRY_VERIFY(window->isExposed());
    auto *tree = requiredObject<QQuickItem>(window, "treeScene");
    auto *reducedMotion = requiredObject<QQuickItem>(
        window, "reducedMotionCheckBox");
    auto *travelForward = requiredObject<QQuickItem>(
        window, "travelForwardButton");
    auto *up = requiredObject<QQuickItem>(window, "upButton");
    auto *back = requiredObject<QQuickItem>(window, "backButton");
    QVERIFY(tree);
    QVERIFY(reducedMotion);
    QVERIFY(travelForward);
    QVERIFY(up);
    QVERIFY(back);

    QTRY_COMPARE(tree->property("breadcrumb").toString(),
                 fixture.directory.path());
    clickItem(window, reducedMotion);
    QTRY_VERIFY(tree->property("reducedMotion").toBool());

    bool navigated = false;
    QVERIFY(QMetaObject::invokeMethod(
        tree, "navigateToPath",
        Q_RETURN_ARG(bool, navigated),
        Q_ARG(QString, fixture.branchPath)));
    QVERIFY(navigated);
    QTRY_COMPARE(tree->property("breadcrumb").toString(),
                 fixture.branchPath);
    QTRY_VERIFY(realProperty(tree, "maximumTravelDepth") > 0.0);
    clickItem(window, travelForward);
    QVERIFY(realProperty(tree, "travelDepth") > 0.0);

    QTRY_VERIFY(up->property("enabled").toBool());
    clickItem(window, up);
    QTRY_COMPARE(tree->property("breadcrumb").toString(),
                 fixture.directory.path());
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));

    QTRY_VERIFY(back->property("enabled").toBool());
    clickItem(window, back);
    QTRY_COMPARE(tree->property("breadcrumb").toString(),
                 fixture.branchPath);
    QCOMPARE(realProperty(tree, "travelDepth"), qreal(0.0));
    QVERIFY2(loaded.warnings.isEmpty(),
             qPrintable(warningText(loaded.warnings)));
}

int main(int argc, char *argv[])
{
    QTemporaryDir syntheticHome;
    if (!syntheticHome.isValid())
        return EXIT_FAILURE;

    const QString homePath = syntheticHome.path();
    const QString configPath = homePath + QStringLiteral("/config");
    const QString cachePath = homePath + QStringLiteral("/cache");
    const QString dataPath = homePath + QStringLiteral("/data");
    const QString runtimePath = homePath + QStringLiteral("/runtime");
    QDir().mkpath(configPath);
    QDir().mkpath(cachePath);
    QDir().mkpath(dataPath);
    QDir().mkpath(runtimePath);
    QFile::setPermissions(
        runtimePath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner
            | QFileDevice::ExeOwner);
    qputenv("HOME", homePath.toUtf8());
    qputenv("XDG_CONFIG_HOME", configPath.toUtf8());
    qputenv("XDG_CACHE_HOME", cachePath.toUtf8());
    qputenv("XDG_DATA_HOME", dataPath.toUtf8());
    qputenv("XDG_RUNTIME_DIR", runtimePath.toUtf8());

    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("CanopyQmlTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("Canopy"));
    if (QDir::homePath() != homePath)
        return EXIT_FAILURE;

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<TreeSceneItem>("Canopy", 1, 0, "TreeScene");
    qmlRegisterSingletonType<ThemeTokens>(
        "Canopy", 1, 0, "Theme",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ThemeTokens;
        });

    MainQmlInteractionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainQmlInteractionTest.moc"
