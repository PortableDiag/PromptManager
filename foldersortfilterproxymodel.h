#ifndef FOLDERSORTFILTERPROXYMODEL_H
#define FOLDERSORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QHash>
#include <QDateTime>

// Proxy for the Folders tree. Adds two things the plain proxy didn't do:
//  * Search that matches folder names AND prompt titles, keeping the matched
//    node's ancestors (so you can see where it lives) and, for a matched
//    folder, its contents.
//  * Auto-sort of FOLDERS only (Name A-Z/Z-A, or Newest/Oldest by the most
//    recent prompt inside). Prompts keep their manual drag-and-drop order.
class FolderSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    // Order must match the sort combo's item order.
    enum SortMode { Manual = 0, NameAsc, NameDesc, Newest, Oldest };

    explicit FolderSortFilterProxyModel(QObject *parent = nullptr);

    void setSortMode(SortMode mode);
    SortMode sortMode() const { return m_mode; }

    void setFilterText(const QString &text);

    // id -> prompt modified time; drives the Newest/Oldest folder sort.
    void setPromptTimes(const QHash<QString, QDateTime> &times);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool isFolder(const QModelIndex &srcIndex) const;
    bool nodeMatches(const QModelIndex &srcIndex) const;
    bool descendantMatches(const QModelIndex &srcIndex) const;
    bool ancestorMatches(const QModelIndex &srcParent) const;
    // Newest prompt-modified time in a subtree (invalid if the folder is empty).
    QDateTime folderLatest(const QModelIndex &srcIndex) const;

    void applySort();

    SortMode m_mode = Manual;
    QString m_filter;
    QHash<QString, QDateTime> m_times;
};

#endif // FOLDERSORTFILTERPROXYMODEL_H
