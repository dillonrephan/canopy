#include "DirectoryScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QMimeDatabase>

#include <algorithm>
#include <utility>

namespace {
constexpr qsizetype kGrowthBatchSize = 4;
constexpr quint64 kProgressInterval = 256;

bool entryLess(const ScannedEntry &left, const ScannedEntry &right)
{
    return left.orderingKey < right.orderingKey;
}
}

DirectoryScanResult DirectoryScanner::scan(
    const QString &path,
    qsizetype materializedEntryLimit,
    const std::function<bool()> &shouldCancel,
    const std::function<void(const DirectoryScanUpdate &)> &onUpdate)
{
    DirectoryScanResult result;
    const QFileInfo directoryInfo(path);
    result.path = QDir::cleanPath(directoryInfo.absoluteFilePath());

    if (!directoryInfo.exists()) {
        result.state = DirectoryLoadState::Error;
        result.status = QStringLiteral("This directory no longer exists.");
        return result;
    }
    if (!directoryInfo.isDir()) {
        result.state = DirectoryLoadState::Error;
        result.status = QStringLiteral("This path is not a directory.");
        return result;
    }
    if (!directoryInfo.isReadable()) {
        result.state = DirectoryLoadState::Inaccessible;
        result.status = QStringLiteral("Permission to read this directory was denied.");
        return result;
    }

    materializedEntryLimit = std::max<qsizetype>(1, materializedEntryLimit);
    result.entries.reserve(materializedEntryLimit);
    QVector<ScannedEntry> pendingEntries;
    pendingEntries.reserve(kGrowthBatchSize);

    const QDir::Filters scanPasses[] {
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Files | QDir::System | QDir::NoDotAndDotDot
    };
    for (const QDir::Filters filters : scanPasses) {
        QDirIterator iterator(
            result.path, filters, QDirIterator::NoIteratorFlags);
        while (iterator.hasNext()) {
            if (shouldCancel && shouldCancel()) {
                result.cancelled = true;
                result.status = QStringLiteral("Scan cancelled.");
                return result;
            }

            iterator.next();
            const QFileInfo info = iterator.fileInfo();
            if (!info.exists() && !info.isSymbolicLink())
                continue;

            ++result.totalEntryCount;
            if (result.entries.size() < materializedEntryLimit) {
                ScannedEntry entry = makeEntry(info);
                result.entries.append(entry);
                pendingEntries.append(std::move(entry));
            }

            const bool batchReady = pendingEntries.size() == kGrowthBatchSize;
            const bool progressReady =
                result.totalEntryCount % kProgressInterval == 0;
            if (onUpdate && (batchReady || progressReady)) {
                onUpdate({
                    .path = result.path,
                    .entries = std::exchange(pendingEntries, {}),
                    .discoveredEntryCount = result.totalEntryCount
                });
                pendingEntries.reserve(kGrowthBatchSize);
            }
        }
    }

    if (onUpdate && !pendingEntries.isEmpty()) {
        onUpdate({
            .path = result.path,
            .entries = std::move(pendingEntries),
            .discoveredEntryCount = result.totalEntryCount
        });
    }
    std::sort(result.entries.begin(), result.entries.end(), entryLess);

    if (result.totalEntryCount > static_cast<quint64>(result.entries.size())) {
        result.state = DirectoryLoadState::Partial;
        result.status = QStringLiteral(
            "Showing %1 of %2 entries. The scan remained memory-bounded.")
                            .arg(result.entries.size())
                            .arg(result.totalEntryCount);
    } else {
        result.state = DirectoryLoadState::Ready;
        result.status = result.entries.isEmpty()
            ? QStringLiteral("This directory is empty.")
            : QStringLiteral("%1 entries loaded.").arg(result.totalEntryCount);
    }
    return result;
}

DirectoryPageResult DirectoryScanner::scanPage(
    const QString &path,
    const QString &afterCursor,
    qsizetype pageSize,
    const std::function<bool()> &shouldCancel)
{
    DirectoryPageResult result;
    const QFileInfo directoryInfo(path);
    result.path = QDir::cleanPath(directoryInfo.absoluteFilePath());

    if (!directoryInfo.exists()) {
        result.state = DirectoryLoadState::Error;
        result.status = QStringLiteral("This directory no longer exists.");
        return result;
    }
    if (!directoryInfo.isDir()) {
        result.state = DirectoryLoadState::Error;
        result.status = QStringLiteral("This path is not a directory.");
        return result;
    }
    if (!directoryInfo.isReadable()) {
        result.state = DirectoryLoadState::Inaccessible;
        result.status = QStringLiteral("Permission to read this directory was denied.");
        return result;
    }

    pageSize = std::max<qsizetype>(1, pageSize);
    result.entries.reserve(pageSize);
    quint64 eligibleEntryCount = 0;
    QDirIterator iterator(
        result.path,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::NoIteratorFlags);
    while (iterator.hasNext()) {
        if (shouldCancel && shouldCancel()) {
            result.cancelled = true;
            result.status = QStringLiteral("Page load cancelled.");
            return result;
        }

        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (!info.exists() && !info.isSymbolicLink())
            continue;

        ++result.totalEntryCount;
        ScannedEntry entry = makeEntry(info);
        if (!afterCursor.isEmpty() && entry.orderingKey <= afterCursor)
            continue;

        ++eligibleEntryCount;
        const auto position = std::lower_bound(
            result.entries.begin(), result.entries.end(), entry, entryLess);
        result.entries.insert(position, std::move(entry));
        if (result.entries.size() > pageSize)
            result.entries.removeLast();
    }

    result.hasNextPage =
        eligibleEntryCount > static_cast<quint64>(result.entries.size());
    if (!result.entries.isEmpty())
        result.nextCursor = result.entries.last().orderingKey;
    result.state = DirectoryLoadState::Ready;
    result.status = result.entries.isEmpty()
        ? QStringLiteral("No entries on this page.")
        : QStringLiteral("%1 entries on this page.").arg(result.entries.size());
    return result;
}

ScannedEntry DirectoryScanner::makeEntry(const QFileInfo &entry)
{
    const NodeKind kind = classify(entry);
    const QString category = kind == NodeKind::Directory
        ? QStringLiteral("0") : QStringLiteral("1");
    const QString name = entry.fileName();
    return {
        .name = name,
        .path = entry.absoluteFilePath(),
        .orderingKey = category + QChar(0) + name.toCaseFolded()
            + QChar(0) + name,
        .kind = kind,
        .detail = describe(entry),
        .isSymbolicLink = entry.isSymbolicLink()
    };
}

NodeKind DirectoryScanner::classify(const QFileInfo &entry)
{
    if (entry.isDir())
        return NodeKind::Directory;

    static QMimeDatabase mimeDatabase;
    const QString mimeName = mimeDatabase.mimeTypeForFile(
        entry, QMimeDatabase::MatchExtension).name();
    if (mimeName.startsWith(QStringLiteral("image/")))
        return NodeKind::Image;
    if (mimeName.startsWith(QStringLiteral("audio/")))
        return NodeKind::Audio;
    if (mimeName.startsWith(QStringLiteral("text/"))
        || mimeName.contains(QStringLiteral("json"))
        || mimeName.contains(QStringLiteral("javascript"))
        || mimeName.contains(QStringLiteral("xml"))
        || mimeName.contains(QStringLiteral("yaml"))) {
        const QString suffix = entry.suffix().toLower();
        static const QStringList codeSuffixes {
            QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cpp"),
            QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hpp"),
            QStringLiteral("js"), QStringLiteral("json"), QStringLiteral("py"),
            QStringLiteral("qml"), QStringLiteral("rs"), QStringLiteral("sh"),
            QStringLiteral("toml"), QStringLiteral("ts"), QStringLiteral("xml"),
            QStringLiteral("yaml"), QStringLiteral("yml")
        };
        return codeSuffixes.contains(suffix) ? NodeKind::Code : NodeKind::Document;
    }
    if (mimeName.contains(QStringLiteral("archive"))
        || mimeName.contains(QStringLiteral("compressed"))
        || mimeName.contains(QStringLiteral("zip"))
        || mimeName.contains(QStringLiteral("tar"))) {
        return NodeKind::Archive;
    }
    return NodeKind::Document;
}

QString DirectoryScanner::describe(const QFileInfo &entry)
{
    if (entry.isSymbolicLink()) {
        return QStringLiteral("Symbolic link to %1").arg(entry.symLinkTarget());
    }
    if (entry.isDir())
        return QStringLiteral("Directory");

    const qint64 bytes = entry.size();
    if (bytes < 1024)
        return QStringLiteral("%1 bytes").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024LL * 1024LL)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB")
        .arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}
