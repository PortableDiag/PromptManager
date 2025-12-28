#include "foldertreemodel.h"
#include <QIcon>
#include <QDateTime>
#include <QMimeData>
#include <QDataStream>
#include <QIODevice>

FolderTreeModel::FolderTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem = new FolderTreeItem(FolderTreeItem::FolderType, "Root", "root");
}

FolderTreeModel::~FolderTreeModel()
{
    delete rootItem;
}

QVariant FolderTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    FolderTreeItem *item = getItem(index);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return item->data(0);
    }

    if (role == Qt::DecorationRole) {
        if (item->type() == FolderTreeItem::FolderType) {
            return QIcon::fromTheme("folder");
        } else {
            return QIcon::fromTheme("text-x-generic");
        }
    }

    if (role == Qt::UserRole) {
        return item->id();
    }

    if (role == Qt::UserRole + 1) {
        return static_cast<int>(item->type());
    }

    return QVariant();
}

Qt::ItemFlags FolderTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = QAbstractItemModel::flags(index) | Qt::ItemIsEditable;

    FolderTreeItem *item = getItem(index);
    if (item->type() == FolderTreeItem::FolderType) {
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    } else {
        flags |= Qt::ItemIsDragEnabled;
    }

    return flags;
}

QVariant FolderTreeModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0) {
        return "Prompts";
    }
    return QVariant();
}

QModelIndex FolderTreeModel::index(int row, int column,
                                  const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    FolderTreeItem *parentItem = getItem(parent);
    FolderTreeItem *childItem = parentItem->child(row);

    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex FolderTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    FolderTreeItem *childItem = getItem(index);
    FolderTreeItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int FolderTreeModel::rowCount(const QModelIndex &parent) const
{
    FolderTreeItem *parentItem = getItem(parent);
    return parentItem->childCount();
}

int FolderTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

bool FolderTreeModel::setData(const QModelIndex &index, const QVariant &value,
                             int role)
{
    if (role != Qt::EditRole)
        return false;

    FolderTreeItem *item = getItem(index);
    bool result = item->setData(0, value);

    if (result)
        emit dataChanged(index, index, {role});

    return result;
}

Qt::DropActions FolderTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList FolderTreeModel::mimeTypes() const
{
    return QStringList() << "application/x-promptmanager-item";
}

QMimeData *FolderTreeModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        if (index.isValid()) {
            FolderTreeItem *item = getItem(index);
            stream << static_cast<int>(item->type());
            stream << item->data(0).toString();
            stream << item->id();
        }
    }

    mimeData->setData("application/x-promptmanager-item", encodedData);
    return mimeData;
}

bool FolderTreeModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                      int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (!data->hasFormat("application/x-promptmanager-item"))
        return false;

    if (action != Qt::MoveAction)
        return false;

    // Can only drop on folders or the root
    if (parent.isValid()) {
        FolderTreeItem *parentItem = getItem(parent);
        if (parentItem->type() != FolderTreeItem::FolderType)
            return false;
    }

    return true;
}

bool FolderTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                   int row, int column, const QModelIndex &parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    if (action != Qt::MoveAction)
        return false;

    QByteArray encodedData = data->data("application/x-promptmanager-item");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    
    while (!stream.atEnd()) {
        int itemType;
        QString itemName;
        QString itemId;
        
        stream >> itemType;
        stream >> itemName;
        stream >> itemId;

        // Find the original item to move
        QModelIndex originalIndex = findItemById(itemId);
        if (!originalIndex.isValid())
            continue;

        FolderTreeItem *originalItem = getItem(originalIndex);
        FolderTreeItem *parentItem = getItem(parent);
        
        // Don't move into itself or its descendants
        if (originalItem->type() == FolderTreeItem::FolderType) {
            FolderTreeItem *checkParent = parentItem;
            while (checkParent) {
                if (checkParent == originalItem)
                    return false;
                checkParent = checkParent->parent();
            }
        }

        // Calculate the insertion row
        int insertRow = row;
        if (insertRow == -1) {
            insertRow = parentItem->childCount();
        }

        // If we're moving within the same parent and the item is before the target position,
        // adjust the insertion row
        if (originalItem->parent() == parentItem && originalItem->row() < insertRow) {
            insertRow--;
        }

        // Begin the move operation
        beginMoveRows(originalIndex.parent(), originalItem->row(), originalItem->row(),
                      parent, insertRow);

        // Remove from old parent
        originalItem->parent()->removeChild(originalItem);

        // Insert into new parent
        if (insertRow >= parentItem->childCount()) {
            parentItem->appendChild(originalItem);
        } else {
            parentItem->insertChild(insertRow, originalItem);
        }

        // Update the parent pointer
        originalItem->setParent(parentItem);

        endMoveRows();

        // Emit signal that item was moved
        if (originalItem->type() == FolderTreeItem::PromptType) {
            // Build the folder path for the new location
            QString newFolderPath = buildFolderPath(parentItem);
            emit itemMoved(originalItem->id(), newFolderPath);
        }
    }

    return true;
}

bool FolderTreeModel::insertFolder(const QModelIndex &parent, const QString &name)
{
    FolderTreeItem *parentItem = getItem(parent);

    beginInsertRows(parent, parentItem->childCount(), parentItem->childCount());
    parentItem->appendChild(
        new FolderTreeItem(FolderTreeItem::FolderType, name,
                          "folder_" + QString::number(QDateTime::currentMSecsSinceEpoch()),
                          parentItem));
    endInsertRows();

    return true; // Always successful if we reach here
}

bool FolderTreeModel::insertPrompt(const QModelIndex &parent, const QString &title, const QString &id)
{
    FolderTreeItem *parentItem = getItem(parent);

    beginInsertRows(parent, parentItem->childCount(), parentItem->childCount());
    parentItem->appendChild(
        new FolderTreeItem(FolderTreeItem::PromptType, title, id, parentItem));
    endInsertRows();

    return true; // Always successful if we reach here
}

bool FolderTreeModel::removeItem(const QModelIndex &index)
{
    if (!index.isValid())
        return false;

    FolderTreeItem *item = getItem(index);
    FolderTreeItem *parentItem = item->parent();

    int row = item->row();

    beginRemoveRows(index.parent(), row, row);
    bool success = parentItem->removeChild(item);
    endRemoveRows();

    if (success)
        delete item;

    return success;
}

FolderTreeItem *FolderTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid()) {
        FolderTreeItem *item = static_cast<FolderTreeItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return rootItem;
}

QModelIndex FolderTreeModel::findItemById(const QString &id)
{
    return findItemByIdRecursive(QModelIndex(), id);
}

QModelIndex FolderTreeModel::findItemByIdRecursive(const QModelIndex &parent, const QString &id)
{
    int rows = rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        QModelIndex index = this->index(i, 0, parent);
        FolderTreeItem *item = getItem(index);

        if (item->id() == id)
            return index;

        if (item->type() == FolderTreeItem::FolderType) {
            QModelIndex found = findItemByIdRecursive(index, id);
            if (found.isValid())
                return found;
        }
    }
    return QModelIndex();
}

QString FolderTreeModel::buildFolderPath(FolderTreeItem *item) const
{
    if (!item || item == rootItem || item->type() != FolderTreeItem::FolderType) {
        return "General";  // Default folder
    }

    QStringList pathParts;
    FolderTreeItem *current = item;
    
    while (current && current != rootItem) {
        if (current->type() == FolderTreeItem::FolderType) {
            pathParts.prepend(current->data(0).toString());
        }
        current = current->parent();
    }
    
    return pathParts.join("/");
}

// FolderTreeItem implementation
FolderTreeItem::FolderTreeItem(ItemType type, const QVariant &data,
                              const QString &id, FolderTreeItem *parent)
    : m_type(type), m_data(data), m_id(id), m_parent(parent)
{
}

FolderTreeItem::~FolderTreeItem()
{
    qDeleteAll(m_children);
}

void FolderTreeItem::appendChild(FolderTreeItem *child)
{
    m_children.append(child);
}

void FolderTreeItem::insertChild(int row, FolderTreeItem *child)
{
    if (row >= 0 && row <= m_children.size()) {
        m_children.insert(row, child);
    }
}

bool FolderTreeItem::removeChild(FolderTreeItem *child)
{
    return m_children.removeOne(child);
}

FolderTreeItem *FolderTreeItem::child(int row)
{
    if (row < 0 || row >= m_children.size())
        return nullptr;
    return m_children.at(row);
}

int FolderTreeItem::childCount() const
{
    return m_children.size();
}

int FolderTreeItem::columnCount() const
{
    return 1;
}

QVariant FolderTreeItem::data(int column) const
{
    Q_UNUSED(column);
    return m_data;
}

bool FolderTreeItem::setData(int column, const QVariant &value)
{
    Q_UNUSED(column);
    m_data = value;
    return true;
}

int FolderTreeItem::row() const
{
    if (m_parent)
        return m_parent->m_children.indexOf(const_cast<FolderTreeItem*>(this));
    return 0;
}

FolderTreeItem *FolderTreeItem::parent()
{
    return m_parent;
}

FolderTreeItem::ItemType FolderTreeItem::type() const
{
    return m_type;
}

QString FolderTreeItem::id() const
{
    return m_id;
}

void FolderTreeItem::setId(const QString &id)
{
    m_id = id;
}

void FolderTreeItem::setParent(FolderTreeItem *parent)
{
    m_parent = parent;
}

FolderTreeItem *FolderTreeItem::findChildById(const QString &id)
{
    for (FolderTreeItem *child : m_children) {
        if (child->id() == id)
            return child;
    }
    return nullptr;
}

QVector<FolderTreeItem*> FolderTreeItem::getAllPrompts() const
{
    QVector<FolderTreeItem*> prompts;
    for (FolderTreeItem *child : m_children) {
        if (child->type() == PromptType) {
            prompts.append(child);
        } else if (child->type() == FolderType) {
            prompts.append(child->getAllPrompts());
        }
    }
    return prompts;
}
