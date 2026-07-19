#pragma once

#include "FileNode.h"

#include <QFileInfo>
#include <QString>
#include <QVector>

#include <functional>

struct ScannedEntry {
    QString name;
    QString path;
    NodeKind kind = NodeKind::Document;
    QString detail;
    bool isSymbolicLink = false;
};

struct DirectoryScanResult {
    QString path;
    QVector<ScannedEntry> entries;
    quint64 totalEntryCount = 0;
    DirectoryLoadState state = DirectoryLoadState::Error;
    QString status;
    bool cancelled = false;
};

class DirectoryScanner
{
public:
    static constexpr qsizetype DefaultMaterializedEntryLimit = 256;

    [[nodiscard]] static DirectoryScanResult scan(
        const QString &path,
        qsizetype materializedEntryLimit = DefaultMaterializedEntryLimit,
        const std::function<bool()> &shouldCancel = {});

private:
    [[nodiscard]] static NodeKind classify(const QFileInfo &entry);
    [[nodiscard]] static QString describe(const QFileInfo &entry);
};
