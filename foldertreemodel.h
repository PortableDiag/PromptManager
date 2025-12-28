#ifndef FOLDERTREEMODEL_H
#define FOLDERTREEMODEL_H

#include <QAbstractItemModel>
#include "foldertreeitem.h"

class FolderTreeModel : public QAbstractItemModel
{
    Q_OBJECT

signals:
    void itemMoved(const QString &itemId, const QString &newFolderPath);

public:
    explicit FolderTreeModel(QObject *parent = nullptr);
    ~FolderTreeModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                     const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                int role = Qt::EditRole) override;

    // Drag and drop support
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action,
                         int row, int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    // Custom methods
    bool insertFolder(const QModelIndex &parent, const QString &name);
    bool insertPrompt(const QModelIndex &parent, const QString &title, const QString &id);
    bool removeItem(const QModelIndex &index);
    FolderTreeItem *getItem(const QModelIndex &index) const;
    QModelIndex findItemById(const QString &id);

private:
    QModelIndex findItemByIdRecursive(const QModelIndex &parent, const QString &id);
    QString buildFolderPath(FolderTreeItem *item) const;

    FolderTreeItem *rootItem;
};

#endif // FOLDERTREEMODEL_H
