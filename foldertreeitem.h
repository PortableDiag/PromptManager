#ifndef FOLDERTREEITEM_H
#define FOLDERTREEITEM_H

#include <QVector>
#include <QVariant>

class FolderTreeItem
{
public:
    enum ItemType {
        FolderType,
        PromptType
    };

    explicit FolderTreeItem(ItemType type, const QVariant &data,
                          const QString &id = QString(),
                          FolderTreeItem *parent = nullptr);
    ~FolderTreeItem();

    void appendChild(FolderTreeItem *child);
    void insertChild(int row, FolderTreeItem *child);
    bool removeChild(FolderTreeItem *child);
    FolderTreeItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    bool setData(int column, const QVariant &value);
    int row() const;
    FolderTreeItem *parent();
    ItemType type() const;
    QString id() const;
    void setId(const QString &id);
    void setParent(FolderTreeItem *parent);

    FolderTreeItem *findChildById(const QString &id);
    FolderTreeItem *findFolderByName(const QString &name);
    QVector<FolderTreeItem*> getAllPrompts() const;

private:
    QVector<FolderTreeItem*> m_children;
    ItemType m_type;
    QVariant m_data;
    QString m_id;
    FolderTreeItem *m_parent;
};

#endif // FOLDERTREEITEM_H
