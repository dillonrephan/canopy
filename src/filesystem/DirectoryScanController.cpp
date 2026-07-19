#include "DirectoryScanController.h"

#include <QFutureWatcher>
#include <QPromise>
#include <QtConcurrentRun>

DirectoryScanController::DirectoryScanController(QObject *parent)
    : QObject(parent)
    , m_generation(std::make_shared<std::atomic<quint64>>(0))
{
}

DirectoryScanController::~DirectoryScanController()
{
    cancel();
}

quint64 DirectoryScanController::requestScan(
    const QString &path,
    qsizetype materializedEntryLimit)
{
    const quint64 requestId = m_generation->fetch_add(1) + 1;
    const auto generation = m_generation;
    auto *watcher = new QFutureWatcher<DirectoryScanUpdate>(this);

    connect(watcher, &QFutureWatcher<DirectoryScanUpdate>::resultsReadyAt,
            this, [this, watcher, requestId, generation](
                      qsizetype begin, qsizetype end) {
                if (generation->load() != requestId)
                    return;
                for (qsizetype index = begin; index < end; ++index) {
                    const DirectoryScanUpdate update = watcher->resultAt(index);
                    if (update.complete)
                        emit scanFinished(requestId, update.finalResult);
                    else
                        emit scanUpdated(requestId, update);
                }
            });
    connect(watcher, &QFutureWatcher<DirectoryScanUpdate>::finished,
            this, [watcher] {
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run(
        [path, materializedEntryLimit, requestId, generation](
            QPromise<DirectoryScanUpdate> &promise) {
            const DirectoryScanResult result = DirectoryScanner::scan(
                path,
                materializedEntryLimit,
                [requestId, generation] {
                    return generation->load() != requestId;
                },
                [&promise](const DirectoryScanUpdate &update) {
                    promise.addResult(update);
                });
            if (!result.cancelled && generation->load() == requestId) {
                promise.addResult({
                    .path = result.path,
                    .discoveredEntryCount = result.totalEntryCount,
                    .complete = true,
                    .finalResult = result
                });
            }
        }));
    emit scanStarted(requestId, path);
    return requestId;
}

void DirectoryScanController::cancel()
{
    m_generation->fetch_add(1);
}
