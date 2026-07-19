#include "DirectoryScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QMimeDatabase>

#include <algorithm>

namespace {
bool entryLess(const ScannedEntry &left, const ScannedEntry &right)
{
    if (left.kind == NodeKind::Directory && right.kind != NodeKind::Directory)
        return true;
    if (left.kind != NodeKind::Directory && right.kind == NodeKind::Directory)
        return false;

    const int insensitive = QString::compare(
        left.name, right.name, Qt::CaseInsensitive);
    return insensitive == 0 ? left.name < right.name : insensitive < 0;
}
}

DirectoryScanResult DirectoryScanner::scan(
    const QString &path,
    qsizetype materializedEntryLimit,
    const std::function<bool()> &shouldCancel)
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

    QDirIterator iterator(
        result.path,
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDirIterator::NoIteratorFlags);
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
        ScannedEntry entry {
            .name = info.fileName(),
            .path = info.absoluteFilePath(),
            .kind = classify(info),
            .detail = describe(info),
            .isSymbolicLink = info.isSymbolicLink()
        };

        const auto position = std::lower_bound(
            result.entries.begin(), result.entries.end(), entry, entryLess);
        result.entries.insert(position, std::move(entry));
        if (result.entries.size() > materializedEntryLimit)
            result.entries.removeLast();
    }

    if (result.totalEntryCount > static_cast<quint64>(result.entries.size())) {
        result.state = DirectoryLoadState::Partial;
        result.status = QStringLiteral(
            "Showing the first %1 of %2 entries. The scan remained memory-bounded.")
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
