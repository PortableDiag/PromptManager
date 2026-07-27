#include "foldersortfilterproxymodel.h"

// Roles exposed by FolderTreeModel: UserRole = id, UserRole+1 = type
// (0 == FolderType, 1 == PromptType).
static constexpr int TypeRole = Qt::UserRole + 1;

FolderSortFilterProxyModel::FolderSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

bool FolderSortFilterProxyModel::isFolder(const QModelIndex &srcIndex) const
{
    return srcIndex.isValid()
        && sourceModel()->data(srcIndex, TypeRole).toInt() == 0;
}

bool FolderSortFilterProxyModel::nodeMatches(const QModelIndex &srcIndex) const
{
    const QString text = sourceModel()->data(srcIndex, Qt::DisplayRole).toString();
    return text.contains(m_filter, Qt::CaseInsensitive);
}

bool FolderSortFilterProxyModel::descendantMatches(const QModelIndex &srcIndex) const
{
    const int rows = sourceModel()->rowCount(srcIndex);
    for (int i = 0; i < rows; ++i) {
        const QModelIndex child = sourceModel()->index(i, 0, srcIndex);
        if (nodeMatches(child) || descendantMatches(child))
            return true;
    }
    return false;
}

bool FolderSortFilterProxyModel::ancestorMatches(const QModelIndex &srcParent) const
{
    QModelIndex idx = srcParent;
    while (idx.isValid()) {
        if (nodeMatches(idx))
            return true;
        idx = idx.parent();
    }
    return false;
}

bool FolderSortFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                  const QModelIndex &sourceParent) const
{
    if (m_filter.isEmpty())
        return true;

    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid())
        return false;

    // Keep a node if it matches, if it lives under a matched folder (so a
    // folder's contents stay visible), or if something under it matches (so
    // the path down to a match stays visible).
    return nodeMatches(idx)
        || ancestorMatches(sourceParent)
        || descendantMatches(idx);
}

QDateTime FolderSortFilterProxyModel::folderLatest(const QModelIndex &srcIndex) const
{
    if (!isFolder(srcIndex)) {
        const QString id = sourceModel()->data(srcIndex, Qt::UserRole).toString();
        return m_times.value(id);
    }

    QDateTime best; // invalid
    const int rows = sourceModel()->rowCount(srcIndex);
    for (int i = 0; i < rows; ++i) {
        const QDateTime child = folderLatest(sourceModel()->index(i, 0, srcIndex));
        if (child.isValid() && (!best.isValid() || child > best))
            best = child;
    }
    return best;
}

bool FolderSortFilterProxyModel::lessThan(const QModelIndex &left,
                                          const QModelIndex &right) const
{
    // Sort order is always applied as Ascending (see applySort), so lessThan
    // returns the final desired order directly and asc/desc lives in m_mode.
    const bool lf = isFolder(left);
    const bool rf = isFolder(right);

    // Folders always sit above prompts, regardless of mode.
    if (lf != rf)
        return lf;

    // Two prompts: never reorder them - preserve manual drag-and-drop order.
    if (!lf)
        return left.row() < right.row();

    // Two folders: order by the selected mode.
    switch (m_mode) {
    case NameAsc:
    case NameDesc: {
        const QString a = sourceModel()->data(left, Qt::DisplayRole).toString();
        const QString b = sourceModel()->data(right, Qt::DisplayRole).toString();
        const int cmp = QString::compare(a, b, Qt::CaseInsensitive);
        return (m_mode == NameAsc) ? cmp < 0 : cmp > 0;
    }
    case Newest:
    case Oldest: {
        const QDateTime a = folderLatest(left);
        const QDateTime b = folderLatest(right);
        if (a.isValid() != b.isValid()) {
            // A folder with prompts always ranks "newer" than an empty one.
            const bool aHasContent = a.isValid();
            return (m_mode == Newest) ? aHasContent : !aHasContent;
        }
        if (!a.isValid() && !b.isValid())
            return left.row() < right.row();
        return (m_mode == Newest) ? a > b : a < b;
    }
    case Manual:
    default:
        return left.row() < right.row();
    }
}

void FolderSortFilterProxyModel::applySort()
{
    if (m_mode == Manual)
        sort(-1); // reset to source order
    else
        sort(0, Qt::AscendingOrder);
}

void FolderSortFilterProxyModel::setSortMode(SortMode mode)
{
    m_mode = mode;
    invalidate();
    applySort();
}

void FolderSortFilterProxyModel::setFilterText(const QString &text)
{
    m_filter = text.trimmed();
    invalidateFilter();
}

void FolderSortFilterProxyModel::setPromptTimes(const QHash<QString, QDateTime> &times)
{
    m_times = times;
    // Re-assert the sort: times feed Newest/Oldest, and this call also runs
    // after the source model is rebuilt on a replace-import, so it keeps any
    // active sort (including Name A-Z/Z-A) applied to the fresh tree.
    if (m_mode != Manual) {
        invalidate();
        applySort();
    }
}
