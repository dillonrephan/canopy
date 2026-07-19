#include "DirectoryContentsModel.h"

#include <QFutureWatcher>
#include <QtConcurrentRun>

DirectoryContentsModel::DirectoryContentsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_generation(std::make_shared<std::atomic<quint64>>(0))
{
    m_pageCursors.append(QString());
}

DirectoryContentsModel::~DirectoryContentsModel()
{
    m_generation->fetch_add(1);
}

int DirectoryContentsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant DirectoryContentsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const ScannedEntry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return entry.name;
    case PathRole: return entry.path;
    case DetailRole: return entry.detail;
    case DirectoryRole: return entry.kind == NodeKind::Directory;
    case SymbolicLinkRole: return entry.isSymbolicLink;
    default: return {};
    }
}

QHash<int, QByteArray> DirectoryContentsModel::roleNames() const
{
    return {
        {NameRole, QByteArrayLiteral("entryName")},
        {PathRole, QByteArrayLiteral("entryPath")},
        {DetailRole, QByteArrayLiteral("entryDetail")},
        {DirectoryRole, QByteArrayLiteral("entryIsDirectory")},
        {SymbolicLinkRole, QByteArrayLiteral("entryIsSymbolicLink")}
    };
}

QString DirectoryContentsModel::rangeLabel() const
{
    if (m_loading)
        return QStringLiteral("Loading page %1…").arg(pageNumber());
    if (m_entries.isEmpty())
        return m_totalEntryCount == 0
            ? QStringLiteral("No entries")
            : QStringLiteral("No entries on this page");

    const quint64 first =
        static_cast<quint64>(m_pageIndex) * PageSize + 1;
    const quint64 last = first + static_cast<quint64>(m_entries.size()) - 1;
    return QStringLiteral("%1–%2 of %3")
        .arg(first)
        .arg(last)
        .arg(m_totalEntryCount);
}

void DirectoryContentsModel::setDirectory(const QString &path)
{
    if (path == m_directoryPath)
        return;

    m_generation->fetch_add(1);
    m_directoryPath = path;
    m_pageIndex = 0;
    m_pageCursors = {QString()};
    m_totalEntryCount = 0;
    m_hasNextPage = false;
    m_status.clear();
    clearEntries();

    if (m_directoryPath.isEmpty()) {
        m_loading = false;
        emit stateChanged();
        return;
    }
    loadCurrentPage();
}

void DirectoryContentsModel::setIncludeHidden(bool includeHidden)
{
    if (m_includeHidden == includeHidden)
        return;
    m_includeHidden = includeHidden;
    if (!m_directoryPath.isEmpty())
        reload();
}

void DirectoryContentsModel::nextPage()
{
    if (!canNext() || m_entries.isEmpty())
        return;

    const QString cursor = m_entries.last().orderingKey;
    if (m_pageCursors.size() == m_pageIndex + 1)
        m_pageCursors.append(cursor);
    else
        m_pageCursors[m_pageIndex + 1] = cursor;
    ++m_pageIndex;
    loadCurrentPage();
}

void DirectoryContentsModel::previousPage()
{
    if (!canPrevious())
        return;
    --m_pageIndex;
    loadCurrentPage();
}

void DirectoryContentsModel::reload()
{
    if (m_directoryPath.isEmpty())
        return;
    m_generation->fetch_add(1);
    m_pageIndex = 0;
    m_pageCursors = {QString()};
    loadCurrentPage();
}

void DirectoryContentsModel::loadCurrentPage()
{
    const quint64 requestId = m_generation->fetch_add(1) + 1;
    const auto generation = m_generation;
    const QString path = m_directoryPath;
    const QString cursor = m_pageCursors.value(m_pageIndex);
    const bool includeHidden = m_includeHidden;
    m_loading = true;
    m_status = QStringLiteral("Reading a bounded page…");
    clearEntries();
    emit stateChanged();

    auto *watcher = new QFutureWatcher<DirectoryPageResult>(this);
    connect(watcher, &QFutureWatcher<DirectoryPageResult>::finished,
            this, [this, watcher, requestId, generation] {
                const DirectoryPageResult result = watcher->result();
                watcher->deleteLater();
                if (result.cancelled || generation->load() != requestId)
                    return;
                applyPage(result);
            });
    watcher->setFuture(QtConcurrent::run(
        [path, cursor, includeHidden, requestId, generation] {
            return DirectoryScanner::scanPage(
                path, cursor, PageSize, [requestId, generation] {
                    return generation->load() != requestId;
                },
                includeHidden);
        }));
}

void DirectoryContentsModel::applyPage(const DirectoryPageResult &result)
{
    beginResetModel();
    m_entries = result.entries;
    endResetModel();
    m_totalEntryCount = result.totalEntryCount;
    m_hasNextPage = result.hasNextPage;
    m_status = result.status;
    m_loading = false;
    emit stateChanged();
}

void DirectoryContentsModel::clearEntries()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}
