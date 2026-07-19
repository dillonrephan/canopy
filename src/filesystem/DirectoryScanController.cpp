#include "DirectoryScanController.h"

#include <QFutureWatcher>
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

quint64 DirectoryScanController::requestScan(const QString &path)
{
    const quint64 requestId = m_generation->fetch_add(1) + 1;
    const auto generation = m_generation;
    auto *watcher = new QFutureWatcher<DirectoryScanResult>(this);

    connect(watcher, &QFutureWatcher<DirectoryScanResult>::finished,
            this, [this, watcher, requestId, generation] {
                const DirectoryScanResult result = watcher->result();
                watcher->deleteLater();
                if (result.cancelled || generation->load() != requestId)
                    return;
                emit scanFinished(requestId, result);
            });

    watcher->setFuture(QtConcurrent::run(
        [path, requestId, generation] {
            return DirectoryScanner::scan(
                path,
                DirectoryScanner::DefaultMaterializedEntryLimit,
                [requestId, generation] {
                    return generation->load() != requestId;
                });
        }));
    emit scanStarted(requestId, path);
    return requestId;
}

void DirectoryScanController::cancel()
{
    m_generation->fetch_add(1);
}
