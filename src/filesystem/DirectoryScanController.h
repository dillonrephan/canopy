#pragma once

#include "DirectoryScanner.h"

#include <QObject>

#include <atomic>
#include <memory>

class DirectoryScanController final : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryScanController(QObject *parent = nullptr);
    ~DirectoryScanController() override;

    quint64 requestScan(
        const QString &path,
        qsizetype materializedEntryLimit =
            DirectoryScanner::DefaultMaterializedEntryLimit,
        bool includeHidden = false);
    void cancel();

signals:
    void scanStarted(quint64 requestId, const QString &path);
    void scanUpdated(quint64 requestId, const DirectoryScanUpdate &update);
    void scanFinished(quint64 requestId, const DirectoryScanResult &result);

private:
    std::shared_ptr<std::atomic<quint64>> m_generation;
};
