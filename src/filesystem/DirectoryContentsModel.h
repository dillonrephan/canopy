#pragma once

#include "DirectoryScanner.h"

#include <QAbstractListModel>
#include <QtQmlIntegration/qqmlintegration.h>

#include <atomic>
#include <memory>

class DirectoryContentsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY stateChanged)
    Q_PROPERTY(bool canNext READ canNext NOTIFY stateChanged)
    Q_PROPERTY(int pageNumber READ pageNumber NOTIFY stateChanged)
    Q_PROPERTY(qulonglong totalEntryCount READ totalEntryCount NOTIFY stateChanged)
    Q_PROPERTY(QString rangeLabel READ rangeLabel NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        DetailRole,
        DirectoryRole,
        SymbolicLinkRole
    };
    Q_ENUM(Role)

    static constexpr qsizetype PageSize = 64;

    explicit DirectoryContentsModel(QObject *parent = nullptr);
    ~DirectoryContentsModel() override;

    [[nodiscard]] int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool loading() const noexcept { return m_loading; }
    [[nodiscard]] bool canPrevious() const noexcept
    {
        return !m_loading && m_pageIndex > 0;
    }
    [[nodiscard]] bool canNext() const noexcept
    {
        return !m_loading && m_hasNextPage;
    }
    [[nodiscard]] int pageNumber() const noexcept { return m_pageIndex + 1; }
    [[nodiscard]] qulonglong totalEntryCount() const noexcept
    {
        return m_totalEntryCount;
    }
    [[nodiscard]] QString rangeLabel() const;
    [[nodiscard]] QString status() const { return m_status; }
    [[nodiscard]] bool includeHidden() const noexcept { return m_includeHidden; }

    void setDirectory(const QString &path);
    void setIncludeHidden(bool includeHidden);
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void previousPage();
    Q_INVOKABLE void reload();

signals:
    void stateChanged();

private:
    void loadCurrentPage();
    void applyPage(const DirectoryPageResult &result);
    void clearEntries();

    QVector<ScannedEntry> m_entries;
    QVector<QString> m_pageCursors;
    QString m_directoryPath;
    QString m_status;
    std::shared_ptr<std::atomic<quint64>> m_generation;
    quint64 m_totalEntryCount = 0;
    int m_pageIndex = 0;
    bool m_loading = false;
    bool m_hasNextPage = false;
    bool m_includeHidden = false;
};
