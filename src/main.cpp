#include "rendering/TreeSceneItem.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Canopy"));
    QGuiApplication::setOrganizationName(QStringLiteral("Canopy"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("A botanical, read-only filesystem browser."));
    parser.addHelpOption();
    parser.addPositionalArgument(
        QStringLiteral("path"),
        QStringLiteral("Directory to open at startup."),
        QStringLiteral("[path]"));
    parser.process(app);

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QQmlApplicationEngine engine;
    const QStringList positionalArguments = parser.positionalArguments();
    if (!positionalArguments.isEmpty()) {
        engine.setInitialProperties({
            {QStringLiteral("initialPath"), positionalArguments.first()}
        });
    }
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule("Canopy", "Main");

    return app.exec();
}
