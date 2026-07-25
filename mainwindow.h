#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QDateTime>
#include <QJsonObject>
#include "foldertreemodel.h"
#include "apiserver.h"

class QTreeView;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QToolBar;
class QSplitter;
class QStatusBar;
class QMenu;

struct Prompt {
    QString id;
    QString title;
    QString body;
    QString folderPath;
    QDateTime created;
    QDateTime modified;

    QJsonObject toJson() const;
    static Prompt fromJson(const QJsonObject &json);

    QStringList toCsvRow() const;
    static Prompt fromCsvRow(const QStringList &row);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // REST API backend. Each method runs on the GUI thread (invoked by
    // ApiServer via the shared event loop) and returns an HTTP-style result.
    ApiResponse apiListPrompts(const QString &folder, const QString &query);
    ApiResponse apiGetPrompt(const QString &id);
    ApiResponse apiCreatePrompt(const QJsonObject &input);
    ApiResponse apiUpdatePrompt(const QString &id, const QJsonObject &input);
    ApiResponse apiDeletePrompt(const QString &id);
    ApiResponse apiListFolders();
    ApiResponse apiCreateFolder(const QJsonObject &input);
    ApiResponse apiDeleteFolder(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFolderItemSelected(const QModelIndex &current);
    void onPromptItemSelected(const QModelIndex &current);
    void onSearchTextChanged(const QString &text);
    void onSortOrderChanged(int index);

    void onTitleChanged(const QString &text);
    void onBodyChanged();
    void onItemMoved(const QString &itemId, const QString &newFolderPath);

    void newPrompt();
    void newFolder();
    void savePrompt();
    void deleteItem();
    void copyToClipboard();

    void exportToJson();
    void exportToCsv();
    void importFromJson();
    void importFromCsv();
    void exportCurrentFolder();
    void diagnosticCheck();
    void importToCurrentFolder();

    void updateUI();
    void markUnsavedChanges();

    void openApiSettings();
    void showAbout();

    // Right-click menu on the folder tree: copy a node's API path / id.
    void showFolderTreeContextMenu(const QPoint &pos);

private:
    void diagnosticTraverseTree(const QModelIndex &parent, const QString &indent, QStringList &output);
    void collectTreePromptIds(const QModelIndex &parent, QSet<QString> &promptIds);

private:
    void setupUI();
    void setupToolbar();
    void setupMenuBar();
    void setupConnections();

    void loadPrompts();
    void savePrompts();
    bool hasUnsavedChanges() const;
    bool promptBeforeSwitch();

    QString generateId() const;
    int findPromptIndex(const QString &id) const;
    void selectPromptById(const QString &id);
    QString getCurrentFolderPath() const;
    QModelIndex getCurrentFolderIndex() const;

    // Folder path (A/B/C) for an arbitrary proxy index: the folder's own path,
    // or the containing folder's path when the index points at a prompt.
    QString folderPathForIndex(const QModelIndex &proxyIndex) const;
    // Copy to both the system clipboard and the X11 primary selection.
    void copyTextToClipboard(const QString &text, const QString &statusLabel);

    // Walks (creating as needed) the folders named by a "A/B/C" path and
    // returns the source-model index of the deepest folder (root for "").
    QModelIndex ensureFolderPath(const QString &folderPath);

    // API server lifecycle (reads key/port/enabled from QSettings).
    void startApiServerFromSettings();

    void updatePromptList();
    void filterPrompts();

    // Folder deletion helpers
    void countItemsInFolder(FolderTreeItem *folder, int &promptCount, int &folderCount);
    void deleteFolderAndContents(FolderTreeItem *folder);
    void deletePromptsInFolder(FolderTreeItem *folder);

    // Import/Export helper methods
    bool exportJsonToFile(const QString &filename, bool exportAll = true,
                         const QString &folderPath = QString());
    bool exportCsvToFile(const QString &filename, bool exportAll = true,
                        const QString &folderPath = QString());
    bool importJsonFromFile(const QString &filename, bool merge = true,
                           const QString &targetFolder = QString());
    bool importCsvFromFile(const QString &filename, bool merge = true,
                          const QString &targetFolder = QString());

    // UI Components
    QSplitter *mainSplitter;
    QSplitter *leftSplitter;
    QTreeView *folderTreeView;
    QTreeView *promptListView;
    QLineEdit *searchEdit;
    QComboBox *sortCombo;
    QLineEdit *titleEdit;
    QPlainTextEdit *bodyEdit;
    QPushButton *saveButton;
    QPushButton *deleteButton;
    QPushButton *copyButton;
    QPushButton *newFolderButton;
    QPushButton *toggleFoldersButton;
    QPushButton *togglePromptListButton;
    QList<int> lastLeftSplitterSizes;
    QLabel *noPromptLabel;
    QToolBar *toolbar;

    // Models
    FolderTreeModel *folderModel;
    QStandardItemModel *promptListModel;
    QSortFilterProxyModel *folderProxyModel;
    QSortFilterProxyModel *promptProxyModel;

    // Data
    QVector<Prompt> prompts;
    QString currentPromptId;
    Prompt originalPrompt;
    bool isNewPrompt;
    bool hasChanges;

    // REST API server (optional, off by default)
    ApiServer *apiServer;

    // Constants
    static const QString DATA_FILENAME;
};

#endif // MAINWINDOW_H
