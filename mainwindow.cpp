#include "mainwindow.h"
#include <QtWidgets>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QUuid>
#include <QFileDialog>
#include <QTextStream>
#include <QInputDialog>
#include <QMenuBar>
#include <QStringConverter>
#include <QSettings>
#include <QRandomGenerator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include "apiserver.h"

const QString MainWindow::DATA_FILENAME = QDir::homePath() + "/.prompt_snippets_v2.json";

// Generates a random API key of the form "pmk_<32 hex chars>".
static QString generateApiKey()
{
    static const char hex[] = "0123456789abcdef";
    QString key = "pmk_";
    for (int i = 0; i < 32; ++i)
        key += hex[QRandomGenerator::global()->bounded(16)];
    return key;
}

// Prompt structure methods
QJsonObject Prompt::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["title"] = title;
    obj["body"] = body;
    obj["folderPath"] = folderPath;
    obj["created"] = created.toString(Qt::ISODate);
    obj["modified"] = modified.toString(Qt::ISODate);
    return obj;
}

Prompt Prompt::fromJson(const QJsonObject &json) {
    Prompt prompt;
    prompt.id = json["id"].toString();
    prompt.title = json["title"].toString();
    prompt.body = json["body"].toString();
    prompt.folderPath = json["folderPath"].toString();
    prompt.created = QDateTime::fromString(json["created"].toString(), Qt::ISODate);
    prompt.modified = QDateTime::fromString(json["modified"].toString(), Qt::ISODate);

    if (!prompt.created.isValid()) {
        prompt.created = QDateTime::currentDateTime();
    }
    if (!prompt.modified.isValid()) {
        prompt.modified = QDateTime::currentDateTime();
    }

    return prompt;
}

QStringList Prompt::toCsvRow() const {
    QStringList row;
    QString escapedBody = body;
    escapedBody.replace("\n", "\\n").replace("\"", "\"\"");

    row << id
        << title
        << escapedBody
        << folderPath
        << created.toString(Qt::ISODate)
        << modified.toString(Qt::ISODate);
    return row;
}

Prompt Prompt::fromCsvRow(const QStringList &row) {
    Prompt prompt;
    if (row.size() >= 6) {
        prompt.id = row[0];
        prompt.title = row[1];

        QString unescapedBody = row[2];
        prompt.body = unescapedBody.replace("\\n", "\n").replace("\"\"", "\"");

        prompt.folderPath = row[3];
        prompt.created = QDateTime::fromString(row[4], Qt::ISODate);
        prompt.modified = QDateTime::fromString(row[5], Qt::ISODate);

        if (!prompt.created.isValid()) prompt.created = QDateTime::currentDateTime();
        if (!prompt.modified.isValid()) prompt.modified = QDateTime::currentDateTime();
    }
    return prompt;
}

// MainWindow implementation
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , currentPromptId("")
    , isNewPrompt(false)
    , hasChanges(false)
    , apiServer(nullptr)
{
    setupMenuBar();
    setupUI();
    setupToolbar();
    setupConnections();
    loadPrompts();
    updateUI();

    // Restore the last folder sort mode (triggers the sort via the combo signal).
    QSettings sortSettings;
    int savedFolderSort = sortSettings.value("ui/folderSortMode", 0).toInt();
    if (savedFolderSort > 0 && savedFolderSort < folderSortCombo->count())
        folderSortCombo->setCurrentIndex(savedFolderSort);

    startApiServerFromSettings();

    setWindowTitle("Prompt Manager with Import/Export");
    resize(1000, 700);
}

MainWindow::~MainWindow()
{
    // Don't show dialogs in destructor - handle in closeEvent instead
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (hasUnsavedChanges() && !currentPromptId.isEmpty()) {
        if (!promptBeforeSwitch()) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    centralWidget->setStyleSheet(R"(
        QLineEdit, QPlainTextEdit, QTreeView {
            background-color: white;
            color: black;
            border: 1px solid #ccc;
            border-radius: 3px;
        }

        QLineEdit:disabled, QPlainTextEdit:disabled {
            background-color: #f0f0f0;
            color: #666666;
        }

        QPushButton {
            padding: 8px 15px;
            border-radius: 4px;
            border: 1px solid #ccc;
        }
    )");

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);

    leftSplitter = new QSplitter(Qt::Vertical);

    QWidget *folderWidget = new QWidget;
    QVBoxLayout *folderLayout = new QVBoxLayout(folderWidget);
    folderLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *folderHeaderLayout = new QHBoxLayout;
    folderHeaderLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *folderLabel = new QLabel("Folders");
    folderLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #e0e0e0;");
    folderHeaderLayout->addWidget(folderLabel);
    folderHeaderLayout->addStretch();

    toggleFoldersButton = new QPushButton("Collapse All");
    toggleFoldersButton->setToolTip("Collapse or expand all folders");
    toggleFoldersButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #e0e0e0;
            border: 1px solid #555;
            padding: 2px 8px;
            border-radius: 3px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
        QPushButton:pressed {
            background-color: #2a2a2a;
        }
    )");
    folderHeaderLayout->addWidget(toggleFoldersButton);

    folderLayout->addLayout(folderHeaderLayout);

    folderModel = new FolderTreeModel(this);
    folderProxyModel = new FolderSortFilterProxyModel(this);
    folderProxyModel->setSourceModel(folderModel);

    // Folder search + auto-sort. Search matches folder names and prompt
    // titles; the sort reorders folders only (prompts stay drag-orderable).
    QHBoxLayout *folderToolsLayout = new QHBoxLayout;
    folderToolsLayout->setContentsMargins(0, 0, 0, 4);

    folderSearchEdit = new QLineEdit;
    folderSearchEdit->setPlaceholderText("Search folders & prompts...");
    folderSearchEdit->setClearButtonEnabled(true);

    folderSortCombo = new QComboBox;
    folderSortCombo->setToolTip("Sort folders (prompts keep their manual order)");
    folderSortCombo->addItem("Manual");
    folderSortCombo->addItem("Name A-Z");
    folderSortCombo->addItem("Name Z-A");
    folderSortCombo->addItem("Newest");
    folderSortCombo->addItem("Oldest");

    folderToolsLayout->addWidget(folderSearchEdit, 1);
    folderToolsLayout->addWidget(folderSortCombo);
    folderLayout->addLayout(folderToolsLayout);

    folderTreeView = new QTreeView;
    folderTreeView->setModel(folderProxyModel);
    folderTreeView->setHeaderHidden(true);
    folderTreeView->setDragDropMode(QAbstractItemView::InternalMove);
    folderTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
    folderTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(folderTreeView, &QWidget::customContextMenuRequested,
            this, &MainWindow::showFolderTreeContextMenu);
    folderLayout->addWidget(folderTreeView);
    
    // Connect drag-drop signal
    connect(folderModel, &FolderTreeModel::itemMoved,
            this, &MainWindow::onItemMoved);

    newFolderButton = new QPushButton("New Folder");
    newFolderButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e2944a;
            color: white;
            font-weight: bold;
            border: 1px solid #d2843a;
            padding: 8px 15px;
            border-radius: 4px;
            margin: 5px;
        }
        QPushButton:hover {
            background-color: #d2843a;
        }
        QPushButton:pressed {
            background-color: #c2742a;
        }
    )");
    folderLayout->addWidget(newFolderButton);

    togglePromptListButton = new QPushButton("Show Prompt List");
    togglePromptListButton->setToolTip("Collapse or expand the prompt list panel below");
    togglePromptListButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: #e0e0e0;
            border: 1px solid #555;
            padding: 2px 8px;
            border-radius: 3px;
            margin: 0px 5px 2px 5px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
        QPushButton:pressed {
            background-color: #2a2a2a;
        }
    )");
    folderLayout->addWidget(togglePromptListButton);

    leftSplitter->addWidget(folderWidget);

    QWidget *promptListWidget = new QWidget;
    QVBoxLayout *promptListLayout = new QVBoxLayout(promptListWidget);
    promptListLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *searchLayout = new QHBoxLayout;

    QLabel *searchLabel = new QLabel("Search:");
    searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("Search prompts...");

    QLabel *sortLabel = new QLabel("Sort:");
    sortCombo = new QComboBox;
    sortCombo->addItem("Name A-Z");
    sortCombo->addItem("Name Z-A");
    sortCombo->addItem("Newest First");
    sortCombo->addItem("Oldest First");

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(sortLabel);
    searchLayout->addWidget(sortCombo);

    promptListLayout->addLayout(searchLayout);

    promptListModel = new QStandardItemModel(this);
    promptListModel->setHorizontalHeaderLabels({"Prompts"});

    promptProxyModel = new QSortFilterProxyModel(this);
    promptProxyModel->setSourceModel(promptListModel);
    promptProxyModel->setFilterKeyColumn(0);

    promptListView = new QTreeView;
    promptListView->setModel(promptProxyModel);
    promptListView->setHeaderHidden(true);
    promptListView->setSelectionMode(QAbstractItemView::SingleSelection);
    promptListLayout->addWidget(promptListView);

    leftSplitter->addWidget(promptListWidget);
    leftSplitter->setSizes({1, 0});
    lastLeftSplitterSizes = {200, 400};

    mainSplitter->addWidget(leftSplitter);

    QWidget *rightPanel = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(5, 5, 5, 5);
    rightLayout->setSpacing(10);

    QLabel *titleLabel = new QLabel("Title");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #e0e0e0;");
    rightLayout->addWidget(titleLabel);

    titleEdit = new QLineEdit;
    titleEdit->setPlaceholderText("Enter prompt title...");
    rightLayout->addWidget(titleEdit);

    QLabel *bodyLabel = new QLabel("Body");
    bodyLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #e0e0e0; margin-top: 10px;");
    rightLayout->addWidget(bodyLabel);

    bodyEdit = new QPlainTextEdit;
    bodyEdit->setPlaceholderText("Enter prompt body...");
    rightLayout->addWidget(bodyEdit, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);

    saveButton = new QPushButton("Save");
    saveButton->setDefault(true);
    saveButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4a90e2;
            color: white;
            font-weight: bold;
            border: 1px solid #3a80d2;
            padding: 8px 15px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #3a80d2;
        }
        QPushButton:pressed {
            background-color: #2a70c2;
        }
        QPushButton:disabled {
            background-color: #a0c0e2;
            color: #e0e0e0;
        }
    )");

    deleteButton = new QPushButton("Delete");
    deleteButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e24a4a;
            color: white;
            font-weight: bold;
            border: 1px solid #d23a3a;
            padding: 8px 15px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #d23a3a;
        }
        QPushButton:pressed {
            background-color: #c22a2a;
        }
        QPushButton:disabled {
            background-color: #e2a0a0;
            color: #e0e0e0;
        }
    )");

    copyButton = new QPushButton("Copy to Clipboard");
    copyButton->setStyleSheet(R"(
        QPushButton {
            background-color: #4ae24a;
            color: white;
            font-weight: bold;
            border: 1px solid #3ad23a;
            padding: 8px 15px;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: #3ad23a;
        }
        QPushButton:pressed {
            background-color: #2ac22a;
        }
        QPushButton:disabled {
            background-color: #a0e2a0;
            color: #e0e0e0;
        }
    )");

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(copyButton);

    rightLayout->addLayout(buttonLayout);

    noPromptLabel = new QLabel("No prompt selected. Select a prompt or create a new one.");
    noPromptLabel->setAlignment(Qt::AlignCenter);
    noPromptLabel->setStyleSheet("color: #aaaaaa; font-style: italic; padding: 20px;");
    rightLayout->addWidget(noPromptLabel);

    mainSplitter->addWidget(rightPanel);
    mainSplitter->setSizes({400, 600});

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->addWidget(mainSplitter);

    statusBar()->showMessage("Ready");
}

void MainWindow::setupToolbar()
{
    toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    QAction *newPromptAction = toolbar->addAction(QIcon::fromTheme("document-new"), "New Prompt");
    QAction *newFolderAction = toolbar->addAction(QIcon::fromTheme("folder-new"), "New Folder");
    QAction *saveAction = toolbar->addAction(QIcon::fromTheme("document-save"), "Save");
    QAction *deleteAction = toolbar->addAction(QIcon::fromTheme("edit-delete"), "Delete");
    QAction *copyAction = toolbar->addAction(QIcon::fromTheme("edit-copy"), "Copy");

    toolbar->addSeparator();

    connect(newPromptAction, &QAction::triggered, this, &MainWindow::newPrompt);
    connect(newFolderAction, &QAction::triggered, this, &MainWindow::newFolder);
    connect(saveAction, &QAction::triggered, this, &MainWindow::savePrompt);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteItem);
    connect(copyAction, &QAction::triggered, this, &MainWindow::copyToClipboard);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    QMenu *fileMenu = menuBar->addMenu("File");

    QAction *newPromptAction = fileMenu->addAction("New Prompt");
    newPromptAction->setShortcut(QKeySequence::New);

    QAction *newFolderAction = fileMenu->addAction("New Folder");

    fileMenu->addSeparator();

    QAction *importAction = fileMenu->addAction("Import...");
    importAction->setShortcut(QKeySequence::Open);

    QMenu *importSubMenu = new QMenu("Import From");
    QAction *importJsonAction = importSubMenu->addAction("JSON File");
    QAction *importCsvAction = importSubMenu->addAction("CSV File");
    importAction->setMenu(importSubMenu);

    QAction *exportAction = fileMenu->addAction("Export...");
    exportAction->setShortcut(QKeySequence::SaveAs);

    QMenu *exportSubMenu = new QMenu("Export To");
    QAction *exportJsonAction = exportSubMenu->addAction("JSON File");
    QAction *exportCsvAction = exportSubMenu->addAction("CSV File");
    exportAction->setMenu(exportSubMenu);

    fileMenu->addSeparator();

    QAction *saveAction = fileMenu->addAction("Save All");
    saveAction->setShortcut(QKeySequence::Save);
    
    QAction *diagnosticAction = fileMenu->addAction("Diagnostic Check");
    diagnosticAction->setShortcut(Qt::CTRL | Qt::Key_D);
    connect(diagnosticAction, &QAction::triggered, this, &MainWindow::diagnosticCheck);

    QAction *exitAction = fileMenu->addAction("Exit");
    exitAction->setShortcut(QKeySequence::Quit);

    connect(newPromptAction, &QAction::triggered, this, &MainWindow::newPrompt);
    connect(newFolderAction, &QAction::triggered, this, &MainWindow::newFolder);
    connect(importJsonAction, &QAction::triggered, this, &MainWindow::importFromJson);
    connect(importCsvAction, &QAction::triggered, this, &MainWindow::importFromCsv);
    connect(exportJsonAction, &QAction::triggered, this, &MainWindow::exportToJson);
    connect(exportCsvAction, &QAction::triggered, this, &MainWindow::exportToCsv);
    connect(saveAction, &QAction::triggered, this, &MainWindow::savePrompts);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    QMenu *editMenu = menuBar->addMenu("Edit");

    QAction *copyAction = editMenu->addAction("Copy to Clipboard");
    copyAction->setShortcut(QKeySequence::Copy);

    QAction *deleteAction = editMenu->addAction("Delete");
    deleteAction->setShortcut(QKeySequence::Delete);

    editMenu->addSeparator();

    QAction *exportFolderAction = editMenu->addAction("Export Current Folder...");
    QAction *importToFolderAction = editMenu->addAction("Import to Current Folder...");

    connect(copyAction, &QAction::triggered, this, &MainWindow::copyToClipboard);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteItem);
    connect(exportFolderAction, &QAction::triggered, this, &MainWindow::exportCurrentFolder);
    connect(importToFolderAction, &QAction::triggered, this, &MainWindow::importToCurrentFolder);

    QMenu *settingsMenu = menuBar->addMenu("Settings");
    QAction *apiAction = settingsMenu->addAction("API Server...");
    connect(apiAction, &QAction::triggered, this, &MainWindow::openApiSettings);

    QMenu *helpMenu = menuBar->addMenu("Help");
    QAction *aboutAction = helpMenu->addAction("About Prompt Manager");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::showAbout()
{
    const QString version = QApplication::applicationVersion();
    QMessageBox about(this);
    about.setWindowTitle("About Prompt Manager");
    about.setIconPixmap(windowIcon().pixmap(64, 64));
    about.setTextFormat(Qt::RichText);
    about.setText(QString("<b>Prompt Manager</b><br>Version %1").arg(version));
    about.setInformativeText(
        "Organize and manage reusable text prompts in a hierarchical folder "
        "structure, with a local REST API for agents and tools.");
    about.setStandardButtons(QMessageBox::Ok);
    about.exec();
}

void MainWindow::setupConnections()
{
    connect(folderTreeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onFolderItemSelected);
    connect(promptListView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onPromptItemSelected);

    connect(searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);
    connect(sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSortOrderChanged);
    connect(folderSearchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onFolderSearchChanged);
    connect(folderSortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFolderSortChanged);

    connect(titleEdit, &QLineEdit::textChanged,
            this, &MainWindow::onTitleChanged);
    connect(bodyEdit, &QPlainTextEdit::textChanged,
            this, &MainWindow::onBodyChanged);

    connect(saveButton, &QPushButton::clicked,
            this, &MainWindow::savePrompt);
    connect(deleteButton, &QPushButton::clicked,
            this, &MainWindow::deleteItem);
    connect(copyButton, &QPushButton::clicked,
            this, &MainWindow::copyToClipboard);
    connect(newFolderButton, &QPushButton::clicked,
            this, &MainWindow::newFolder);

    connect(toggleFoldersButton, &QPushButton::clicked, this, [this]() {
        if (toggleFoldersButton->text() == "Collapse All") {
            folderTreeView->collapseAll();
            toggleFoldersButton->setText("Expand All");
        } else {
            folderTreeView->expandAll();
            toggleFoldersButton->setText("Collapse All");
        }
    });

    connect(togglePromptListButton, &QPushButton::clicked, this, [this]() {
        QList<int> sizes = leftSplitter->sizes();
        if (sizes.size() < 2) return;
        if (sizes[1] == 0) {
            QList<int> restore = lastLeftSplitterSizes;
            if (restore.size() != 2 || (restore[0] == 0 && restore[1] == 0)) {
                int total = sizes[0] + sizes[1];
                if (total <= 0) total = leftSplitter->height();
                restore = {total / 3, (total * 2) / 3};
            }
            leftSplitter->setSizes(restore);
            togglePromptListButton->setText("Hide Prompt List");
        } else {
            lastLeftSplitterSizes = sizes;
            leftSplitter->setSizes({sizes[0] + sizes[1], 0});
            togglePromptListButton->setText("Show Prompt List");
        }
    });

    connect(leftSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        QList<int> sizes = leftSplitter->sizes();
        if (sizes.size() < 2) return;
        if (sizes[1] == 0) {
            togglePromptListButton->setText("Show Prompt List");
        } else {
            togglePromptListButton->setText("Hide Prompt List");
            lastLeftSplitterSizes = sizes;
        }
    });
}

void MainWindow::loadPrompts()
{
    QFile file(DATA_FILENAME);

    if (!file.exists()) {
        folderModel->insertFolder(QModelIndex(), "General");
        statusBar()->showMessage("No existing prompts found. Created default folder.");
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Warning",
            QString("Cannot open file %1: %2").arg(DATA_FILENAME).arg(file.errorString()));
        folderModel->insertFolder(QModelIndex(), "General");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Warning",
            QString("Error parsing JSON: %1").arg(parseError.errorString()));
        folderModel->insertFolder(QModelIndex(), "General");
        return;
    }

    if (!doc.isObject()) {
        QMessageBox::warning(this, "Warning", "Invalid data format.");
        folderModel->insertFolder(QModelIndex(), "General");
        return;
    }

    QJsonObject root = doc.object();
    prompts.clear();

    if (root.contains("prompts") && root["prompts"].isArray()) {
        QJsonArray promptArray = root["prompts"].toArray();

        for (const QJsonValue &value : promptArray) {
            if (value.isObject()) {
                Prompt prompt = Prompt::fromJson(value.toObject());
                prompts.append(prompt);

                QString folderPath = prompt.folderPath;
                QModelIndex folderIndex = QModelIndex();

                if (!folderPath.isEmpty()) {
                    QStringList pathParts = folderPath.split('/');
                    
                    // Check if the last part of the path is the same as the prompt title
                    // If so, it's likely a mistake - the prompt shouldn't create a folder with its own name
                    if (!pathParts.isEmpty() && pathParts.last() == prompt.title) {
                        pathParts.removeLast();
                    }
                    
                    for (const QString &part : pathParts) {
                        if (!part.isEmpty()) {
                            bool found = false;
                            for (int i = 0; i < folderModel->rowCount(folderIndex); ++i) {
                                QModelIndex idx = folderModel->index(i, 0, folderIndex);
                                if (folderModel->data(idx, Qt::DisplayRole).toString() == part) {
                                    folderIndex = idx;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                folderModel->insertFolder(folderIndex, part);
                                folderIndex = folderModel->index(folderModel->rowCount(folderIndex) - 1, 0, folderIndex);
                            }
                        }
                    }
                }

                folderModel->insertPrompt(folderIndex, prompt.title, prompt.id);
            }
        }
    }

    // Clean up incorrect folder paths where the prompt name is included in the path
    bool dataChanged = false;
    for (Prompt &prompt : prompts) {
        if (!prompt.folderPath.isEmpty()) {
            QStringList pathParts = prompt.folderPath.split('/');
            if (!pathParts.isEmpty() && pathParts.last() == prompt.title) {
                // Remove the last part (the prompt title) from the folder path
                pathParts.removeLast();
                prompt.folderPath = pathParts.join('/');
                dataChanged = true;
            }
        }
    }
    
    if (dataChanged) {
        savePrompts();
        statusBar()->showMessage(QString("Loaded %1 prompts (fixed folder paths)").arg(prompts.size()));
    } else {
        statusBar()->showMessage(QString("Loaded %1 prompts").arg(prompts.size()));
    }

    refreshFolderSortTimes();
    folderTreeView->expandAll();
}

static void collectPromptIdsInTreeOrder(FolderTreeModel *model,
                                        const QModelIndex &parent,
                                        QStringList &ids)
{
    int rows = model->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        QModelIndex idx = model->index(i, 0, parent);
        FolderTreeItem *item = model->getItem(idx);
        if (!item)
            continue;
        if (item->type() == FolderTreeItem::FolderType) {
            collectPromptIdsInTreeOrder(model, idx, ids);
        } else if (item->type() == FolderTreeItem::PromptType) {
            ids.append(item->id());
        }
    }
}

void MainWindow::savePrompts()
{
    QJsonObject root;
    QJsonArray promptArray;

    QStringList orderedIds;
    collectPromptIdsInTreeOrder(folderModel, QModelIndex(), orderedIds);

    QSet<QString> writtenIds;
    for (const QString &id : orderedIds) {
        int idx = findPromptIndex(id);
        if (idx >= 0) {
            promptArray.append(prompts[idx].toJson());
            writtenIds.insert(id);
        }
    }
    for (const Prompt &prompt : prompts) {
        if (!writtenIds.contains(prompt.id))
            promptArray.append(prompt.toJson());
    }

    root["prompts"] = promptArray;
    root["version"] = "2.0";

    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    // Atomic save: write to temporary file first
    QString tempFilename = DATA_FILENAME + ".tmp";
    QFile tempFile(tempFilename);
    
    if (!tempFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error",
            QString("Cannot save file: %1").arg(tempFile.errorString()));
        statusBar()->showMessage("Error saving prompts");
        return;
    }

    qint64 bytesWritten = tempFile.write(data);
    if (bytesWritten != data.size()) {
        tempFile.close();
        tempFile.remove();
        QMessageBox::critical(this, "Error",
            "Failed to write all data to file");
        statusBar()->showMessage("Error saving prompts");
        return;
    }
    
    tempFile.close();

    // Create backup of existing file
    QFile existingFile(DATA_FILENAME);
    if (existingFile.exists()) {
        QString backupFilename = DATA_FILENAME + ".bak";
        QFile::remove(backupFilename); // Remove old backup
        existingFile.copy(backupFilename);
    }

    // Replace original file with temp file
    QFile::remove(DATA_FILENAME);
    if (!tempFile.rename(tempFilename, DATA_FILENAME)) {
        QMessageBox::critical(this, "Error",
            "Failed to save prompts file");
        statusBar()->showMessage("Error saving prompts");
        return;
    }

    statusBar()->showMessage("Prompts saved");

    // Keep the folder Newest/Oldest sort in step with updated modified times.
    refreshFolderSortTimes();
}

bool MainWindow::hasUnsavedChanges() const
{
    return hasChanges;
}

bool MainWindow::promptBeforeSwitch()
{
    if (!hasUnsavedChanges() || currentPromptId.isEmpty()) {
        return true;
    }

    int result = QMessageBox::question(this, "Unsaved Changes",
        "You have unsaved changes. Do you want to save them before switching?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    switch (result) {
    case QMessageBox::Save:
        savePrompt();
        return true;
    case QMessageBox::Discard:
        hasChanges = false;
        return true;
    case QMessageBox::Cancel:
        return false;
    default:
        return true;
    }
}

QString MainWindow::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int MainWindow::findPromptIndex(const QString &id) const
{
    for (int i = 0; i < prompts.size(); ++i) {
        if (prompts[i].id == id) {
            return i;
        }
    }
    return -1;
}

void MainWindow::selectPromptById(const QString &id)
{
    // Implementation depends on your UI structure
}

QString MainWindow::getCurrentFolderPath() const
{
    return folderPathForIndex(folderTreeView->currentIndex());
}

QString MainWindow::folderPathForIndex(const QModelIndex &proxyIndex) const
{
    QModelIndex currentIndex = proxyIndex;
    if (!currentIndex.isValid()) {
        return QString();
    }

    // Convert proxy index to source model index to check item type
    QModelIndex sourceIndex = folderProxyModel->mapToSource(currentIndex);
    if (sourceIndex.isValid()) {
        FolderTreeItem *item = static_cast<FolderTreeItem*>(sourceIndex.internalPointer());
        if (item && item->type() == FolderTreeItem::PromptType) {
            // If a prompt is selected, use its parent folder
            currentIndex = currentIndex.parent();
        }
    }

    QStringList pathParts;
    QModelIndex index = currentIndex;

    while (index.isValid()) {
        // Check if this item is a folder (not a prompt)
        QModelIndex srcIndex = folderProxyModel->mapToSource(index);
        if (srcIndex.isValid()) {
            FolderTreeItem *item = static_cast<FolderTreeItem*>(srcIndex.internalPointer());
            if (item && item->type() == FolderTreeItem::FolderType) {
                QString name = folderProxyModel->data(index, Qt::DisplayRole).toString();
                if (!name.isEmpty() && name != "Root") {
                    pathParts.prepend(name);
                }
            }
        }
        index = folderProxyModel->parent(index);
    }

    return pathParts.join('/');
}

QModelIndex MainWindow::getCurrentFolderIndex() const
{
    return folderTreeView->currentIndex();
}

void MainWindow::updatePromptList()
{
    promptListModel->clear();
    promptListModel->setHorizontalHeaderLabels({"Prompts"});

    QString folderPath = getCurrentFolderPath();

    for (const Prompt &prompt : prompts) {
        if (prompt.folderPath == folderPath) {
            QStandardItem *item = new QStandardItem(prompt.title);
            item->setData(prompt.id, Qt::UserRole);
            promptListModel->appendRow(item);
        }
    }
}

void MainWindow::filterPrompts()
{
    // Implementation for filtering
}

void MainWindow::exportToJson()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        "Export to JSON",
        QDir::homePath() + "/prompts_export.json",
        "JSON Files (*.json);;All Files (*)"
    );

    if (filename.isEmpty()) return;

    if (!filename.endsWith(".json", Qt::CaseInsensitive)) {
        filename += ".json";
    }

    if (exportJsonToFile(filename, true)) {
        QMessageBox::information(this, "Export Successful",
                               "All prompts exported successfully to:\n" + filename);
        statusBar()->showMessage("Export completed");
    } else {
        QMessageBox::warning(this, "Export Failed",
                           "Failed to export prompts.");
        statusBar()->showMessage("Export failed");
    }
}

void MainWindow::exportToCsv()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        "Export to CSV",
        QDir::homePath() + "/prompts_export.csv",
        "CSV Files (*.csv);;All Files (*)"
    );

    if (filename.isEmpty()) return;

    if (!filename.endsWith(".csv", Qt::CaseInsensitive)) {
        filename += ".csv";
    }

    if (exportCsvToFile(filename, true)) {
        QMessageBox::information(this, "Export Successful",
                               "All prompts exported successfully to:\n" + filename);
        statusBar()->showMessage("Export completed");
    } else {
        QMessageBox::warning(this, "Export Failed",
                           "Failed to export prompts.");
        statusBar()->showMessage("Export failed");
    }
}

void MainWindow::importFromJson()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Import from JSON",
        QDir::homePath(),
        "JSON Files (*.json);;All Files (*)"
    );

    if (filename.isEmpty()) return;

    QMessageBox::StandardButton response = QMessageBox::question(
        this,
        "Import Options",
        "How would you like to import?\n\n"
        "Merge: Add imported prompts to existing collection\n"
        "Replace: Replace all existing prompts with imported ones\n"
        "Cancel: Abort import",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
    );

    if (response == QMessageBox::Cancel) return;

    bool merge = (response == QMessageBox::Yes);

    if (importJsonFromFile(filename, merge)) {
        QMessageBox::information(this, "Import Successful",
                               "Prompts imported successfully.");
        statusBar()->showMessage("Import completed");
    } else {
        QMessageBox::warning(this, "Import Failed",
                           "Failed to import prompts.");
        statusBar()->showMessage("Import failed");
    }
}

void MainWindow::importFromCsv()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Import from CSV",
        QDir::homePath(),
        "CSV Files (*.csv);;All Files (*)"
    );

    if (filename.isEmpty()) return;

    QMessageBox::StandardButton response = QMessageBox::question(
        this,
        "Import Options",
        "How would you like to import?\n\n"
        "Merge: Add imported prompts to existing collection\n"
        "Replace: Replace all existing prompts with imported ones\n"
        "Cancel: Abort import",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
    );

    if (response == QMessageBox::Cancel) return;

    bool merge = (response == QMessageBox::Yes);

    if (importCsvFromFile(filename, merge)) {
        QMessageBox::information(this, "Import Successful",
                               "Prompts imported successfully.");
        statusBar()->showMessage("Import completed");
    } else {
        QMessageBox::warning(this, "Import Failed",
                           "Failed to import prompts.");
        statusBar()->showMessage("Import failed");
    }
}

void MainWindow::exportCurrentFolder()
{
    QString folderPath = getCurrentFolderPath();
    QString folderName = folderPath.isEmpty() ? "Root" : folderPath.split('/').last();

    QString defaultFilename = QDir::homePath() + "/" + folderName + "_prompts";

    QStringList formats;
    formats << "JSON (*.json)" << "CSV (*.csv)";

    bool ok;
    QString format = QInputDialog::getItem(
        this,
        "Export Format",
        "Select export format:",
        formats,
        0, false, &ok
    );

    if (!ok) return;

    QString filter = format;
    QString defaultExt = format.contains("JSON") ? ".json" : ".csv";
    QString defaultPath = defaultFilename + defaultExt;

    QString filename = QFileDialog::getSaveFileName(
        this,
        "Export Current Folder",
        defaultPath,
        filter + ";;All Files (*)",
        &filter
    );

    if (filename.isEmpty()) return;

    bool success = false;
    if (format.contains("JSON")) {
        if (!filename.endsWith(".json", Qt::CaseInsensitive)) {
            filename += ".json";
        }
        success = exportJsonToFile(filename, false, folderPath);
    } else {
        if (!filename.endsWith(".csv", Qt::CaseInsensitive)) {
            filename += ".csv";
        }
        success = exportCsvToFile(filename, false, folderPath);
    }

    if (success) {
        QMessageBox::information(this, "Export Successful",
                               "Folder exported successfully to:\n" + filename);
        statusBar()->showMessage("Folder export completed");
    } else {
        QMessageBox::warning(this, "Export Failed",
                           "Failed to export folder.");
        statusBar()->showMessage("Folder export failed");
    }
}

void MainWindow::importToCurrentFolder()
{
    QString targetFolder = getCurrentFolderPath();
    QString folderName = targetFolder.isEmpty() ? "Root" : targetFolder.split('/').last();

    QStringList formats;
    formats << "JSON (*.json)" << "CSV (*.csv)";

    bool ok;
    QString format = QInputDialog::getItem(
        this,
        "Import Format",
        "Select import format:",
        formats,
        0, false, &ok
    );

    if (!ok) return;

    QString filter = format;
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Import to Current Folder: " + folderName,
        QDir::homePath(),
        filter + ";;All Files (*)",
        &filter
    );

    if (filename.isEmpty()) return;

    bool success = false;
    if (format.contains("JSON")) {
        success = importJsonFromFile(filename, true, targetFolder);
    } else {
        success = importCsvFromFile(filename, true, targetFolder);
    }

    if (success) {
        QMessageBox::information(this, "Import Successful",
                               "Prompts imported to current folder.");
        statusBar()->showMessage("Import completed");
    } else {
        QMessageBox::warning(this, "Import Failed",
                           "Failed to import prompts.");
        statusBar()->showMessage("Import failed");
    }
}

bool MainWindow::exportJsonToFile(const QString &filename, bool exportAll, const QString &folderPath)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                           "Cannot open file for writing: " + file.errorString());
        return false;
    }

    QJsonObject root;
    QJsonArray promptsArray;

    int exportedCount = 0;

    for (const Prompt &prompt : prompts) {
        if (exportAll || prompt.folderPath == folderPath) {
            promptsArray.append(prompt.toJson());
            exportedCount++;
        }
    }

    root["prompts"] = promptsArray;
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["exportVersion"] = "2.0";
    root["totalExported"] = exportedCount;

    QJsonDocument doc(root);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

    if (file.write(jsonData) == -1) {
        file.close();
        return false;
    }

    file.close();
    return true;
}

bool MainWindow::exportCsvToFile(const QString &filename, bool exportAll, const QString &folderPath)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                           "Cannot open file for writing: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QStringList headers = {"ID", "Title", "Body", "FolderPath", "Created", "Modified"};
    stream << "\"" << headers.join("\",\"") << "\"\n";

    int exportedCount = 0;

    for (const Prompt &prompt : prompts) {
        if (exportAll || prompt.folderPath == folderPath) {
            QStringList row = prompt.toCsvRow();
            for (int i = 0; i < row.size(); ++i) {
                stream << "\"" << row[i] << "\"";
                if (i < row.size() - 1) {
                    stream << ",";
                }
            }
            stream << "\n";
            exportedCount++;
        }
    }

    file.close();
    return exportedCount > 0;
}

bool MainWindow::importJsonFromFile(const QString &filename, bool merge, const QString &targetFolder)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                           "Cannot open file for reading: " + file.errorString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Parse Error",
                           "Error parsing JSON: " + parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        QMessageBox::warning(this, "Invalid Format",
                           "File does not contain valid JSON object");
        return false;
    }

    QJsonObject root = doc.object();

    if (!root.contains("prompts") || !root["prompts"].isArray()) {
        QMessageBox::warning(this, "Invalid Format",
                           "File does not contain prompts array");
        return false;
    }

    QJsonArray promptsArray = root["prompts"].toArray();
    int importedCount = 0;
    int skippedCount = 0;

    if (!merge) {
        prompts.clear();
        
        // Disconnect the old model before deleting
        folderProxyModel->setSourceModel(nullptr);
        delete folderModel;
        
        folderModel = new FolderTreeModel(this);
        folderProxyModel->setSourceModel(folderModel);
        
        // Reconnect drag-drop signal
        connect(folderModel, &FolderTreeModel::itemMoved,
                this, &MainWindow::onItemMoved);
                
        folderModel->insertFolder(QModelIndex(), "General");
    }

    for (const QJsonValue &value : promptsArray) {
        if (!value.isObject()) continue;

        Prompt prompt = Prompt::fromJson(value.toObject());

        if (!targetFolder.isEmpty()) {
            prompt.folderPath = targetFolder;
        }

        bool duplicate = false;
        for (const Prompt &existing : prompts) {
            if (existing.id == prompt.id) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            prompts.append(prompt);

            QString folderPath = prompt.folderPath;
            QModelIndex folderIndex = QModelIndex();

            if (!folderPath.isEmpty()) {
                QStringList pathParts = folderPath.split('/');
                for (const QString &part : pathParts) {
                    if (!part.isEmpty()) {
                        bool found = false;
                        for (int i = 0; i < folderModel->rowCount(folderIndex); ++i) {
                            QModelIndex idx = folderModel->index(i, 0, folderIndex);
                            if (folderModel->data(idx, Qt::DisplayRole).toString() == part) {
                                folderIndex = idx;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            folderModel->insertFolder(folderIndex, part);
                            folderIndex = folderModel->index(folderModel->rowCount(folderIndex) - 1,
                                                          0, folderIndex);
                        }
                    }
                }
            }

            folderModel->insertPrompt(folderIndex, prompt.title, prompt.id);
            importedCount++;
        } else {
            skippedCount++;
        }
    }

    savePrompts();
    updatePromptList();
    folderTreeView->expandAll();

    QString message = QString("Imported %1 prompts").arg(importedCount);
    if (skippedCount > 0) {
        message += QString(", skipped %1 duplicates").arg(skippedCount);
    }

    statusBar()->showMessage(message);
    return importedCount > 0;
}

bool MainWindow::importCsvFromFile(const QString &filename, bool merge, const QString &targetFolder)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                           "Cannot open file for reading: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QString headerLine = stream.readLine();
    if (headerLine.isEmpty()) {
        QMessageBox::warning(this, "Invalid Format",
                           "File is empty or invalid");
        return false;
    }

    QStringList headers;
    QString currentField;
    bool inQuotes = false;

    for (int i = 0; i < headerLine.length(); ++i) {
        QChar c = headerLine[i];

        if (c == '\"' && (i == 0 || (i > 0 && headerLine[i-1] != '\\'))) {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            headers.append(currentField.trimmed().replace("\"\"", "\""));
            currentField.clear();
        } else {
            currentField += c;
        }
    }
    headers.append(currentField.trimmed().replace("\"\"", "\""));

    QStringList requiredHeaders = {"ID", "Title", "Body"};
    for (const QString &required : requiredHeaders) {
        if (!headers.contains(required)) {
            QMessageBox::warning(this, "Invalid Format",
                               QString("Missing required column: %1").arg(required));
            return false;
        }
    }

    if (!merge) {
        prompts.clear();
        
        // Disconnect the old model before deleting
        folderProxyModel->setSourceModel(nullptr);
        delete folderModel;
        
        folderModel = new FolderTreeModel(this);
        folderProxyModel->setSourceModel(folderModel);
        
        // Reconnect drag-drop signal
        connect(folderModel, &FolderTreeModel::itemMoved,
                this, &MainWindow::onItemMoved);
                
        folderModel->insertFolder(QModelIndex(), "General");
    }

    int importedCount = 0;
    int skippedCount = 0;
    int lineNumber = 1;

    while (!stream.atEnd()) {
        lineNumber++;
        QString line = stream.readLine();
        if (line.trimmed().isEmpty()) continue;

        QStringList fields;
        QString currentField;
        bool inQuotes = false;

        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];

            if (c == '\"' && (i == 0 || (i > 0 && line[i-1] != '\\'))) {
                inQuotes = !inQuotes;
            } else if (c == ',' && !inQuotes) {
                fields.append(currentField.trimmed().replace("\"\"", "\""));
                currentField.clear();
            } else {
                currentField += c;
            }
        }
        fields.append(currentField.trimmed().replace("\"\"", "\""));

        if (fields.size() >= 3) {
            Prompt prompt;
            prompt.id = fields.value(0);
            prompt.title = fields.value(1);

            QString unescapedBody = fields.value(2);
            prompt.body = unescapedBody.replace("\\n", "\n").replace("\"\"", "\"");

            prompt.folderPath = fields.value(3, "");

            if (fields.size() >= 5) {
                prompt.created = QDateTime::fromString(fields[4], Qt::ISODate);
            }
            if (fields.size() >= 6) {
                prompt.modified = QDateTime::fromString(fields[5], Qt::ISODate);
            }

            if (!prompt.created.isValid()) prompt.created = QDateTime::currentDateTime();
            if (!prompt.modified.isValid()) prompt.modified = QDateTime::currentDateTime();

            if (!targetFolder.isEmpty()) {
                prompt.folderPath = targetFolder;
            }

            if (prompt.id.isEmpty()) {
                prompt.id = generateId();
            }

            bool duplicate = false;
            for (const Prompt &existing : prompts) {
                if (existing.id == prompt.id) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                prompts.append(prompt);

                QString folderPath = prompt.folderPath;
                QModelIndex folderIndex = QModelIndex();

                if (!folderPath.isEmpty()) {
                    QStringList pathParts = folderPath.split('/');
                    for (const QString &part : pathParts) {
                        if (!part.isEmpty()) {
                            bool found = false;
                            for (int i = 0; i < folderModel->rowCount(folderIndex); ++i) {
                                QModelIndex idx = folderModel->index(i, 0, folderIndex);
                                if (folderModel->data(idx, Qt::DisplayRole).toString() == part) {
                                    folderIndex = idx;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                folderModel->insertFolder(folderIndex, part);
                                folderIndex = folderModel->index(folderModel->rowCount(folderIndex) - 1,
                                                              0, folderIndex);
                            }
                        }
                    }
                }

                folderModel->insertPrompt(folderIndex, prompt.title, prompt.id);
                importedCount++;
            } else {
                skippedCount++;
            }
        }
    }

    file.close();

    savePrompts();
    updatePromptList();
    folderTreeView->expandAll();

    QString message = QString("Imported %1 prompts").arg(importedCount);
    if (skippedCount > 0) {
        message += QString(", skipped %1 duplicates").arg(skippedCount);
    }

    statusBar()->showMessage(message);
    return importedCount > 0;
}

void MainWindow::onFolderItemSelected(const QModelIndex &current)
{
    if (!current.isValid()) {
        currentPromptId.clear();
        isNewPrompt = false;
        hasChanges = false;
        updateUI();
        return;
    }

    QString id = folderProxyModel->data(current, Qt::UserRole).toString();
    QModelIndex sourceIndex = folderProxyModel->mapToSource(current);
    FolderTreeItem *item = folderModel->getItem(sourceIndex);

    if (item && item->type() == FolderTreeItem::PromptType) {
        int index = findPromptIndex(id);
        if (index == -1) {
            QMessageBox::warning(this, "Error", "Selected prompt not found in data.");
            return;
        }

        const Prompt &prompt = prompts[index];
        currentPromptId = prompt.id;
        originalPrompt = prompt;
        isNewPrompt = false;
        hasChanges = false;

        QSignalBlocker titleBlocker(titleEdit);
        QSignalBlocker bodyBlocker(bodyEdit);

        titleEdit->setText(prompt.title);
        bodyEdit->setPlainText(prompt.body);

        updateUI();
        statusBar()->showMessage("Prompt loaded");
    } else {
        // Folder selected, update prompt list
        updatePromptList();
        currentPromptId.clear();
        updateUI();
    }
}

void MainWindow::onPromptItemSelected(const QModelIndex &current)
{
    if (!current.isValid()) {
        return;
    }

    QString id = promptProxyModel->data(current, Qt::UserRole).toString();
    int index = findPromptIndex(id);

    if (index == -1) {
        return;
    }

    const Prompt &prompt = prompts[index];
    currentPromptId = prompt.id;
    originalPrompt = prompt;
    isNewPrompt = false;
    hasChanges = false;

    QSignalBlocker titleBlocker(titleEdit);
    QSignalBlocker bodyBlocker(bodyEdit);

    titleEdit->setText(prompt.title);
    bodyEdit->setPlainText(prompt.body);

    updateUI();
    statusBar()->showMessage("Prompt loaded");
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    promptProxyModel->setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
    if (!text.isEmpty()) {
        promptListView->expandAll();
    }
}

void MainWindow::onSortOrderChanged(int index)
{
    bool ascending = (index == 0 || index == 2); // A-Z or Newest First
    promptProxyModel->sort(0, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void MainWindow::onFolderSearchChanged(const QString &text)
{
    folderProxyModel->setFilterText(text);
    // Expand so matches deeper in the tree are visible while searching.
    if (!text.trimmed().isEmpty())
        folderTreeView->expandAll();
}

void MainWindow::onFolderSortChanged(int index)
{
    refreshFolderSortTimes(); // ensure Newest/Oldest reflects current data
    folderProxyModel->setSortMode(
        static_cast<FolderSortFilterProxyModel::SortMode>(index));
    QSettings settings;
    settings.setValue("ui/folderSortMode", index);
}

void MainWindow::refreshFolderSortTimes()
{
    QHash<QString, QDateTime> times;
    times.reserve(prompts.size());
    for (const Prompt &p : prompts)
        times.insert(p.id, p.modified);
    folderProxyModel->setPromptTimes(times);
}

void MainWindow::onTitleChanged(const QString &text)
{
    Q_UNUSED(text);
    if (isNewPrompt) {
        hasChanges = !titleEdit->text().trimmed().isEmpty();
    } else if (!currentPromptId.isEmpty()) {
        hasChanges = (titleEdit->text() != originalPrompt.title) ||
                     (bodyEdit->toPlainText() != originalPrompt.body);
    }
    updateUI();
}

void MainWindow::onBodyChanged()
{
    if (isNewPrompt) {
        hasChanges = !titleEdit->text().trimmed().isEmpty();
    } else if (!currentPromptId.isEmpty()) {
        hasChanges = (titleEdit->text() != originalPrompt.title) ||
                     (bodyEdit->toPlainText() != originalPrompt.body);
    }
    updateUI();
}

void MainWindow::onItemMoved(const QString &itemId, const QString &newFolderPath)
{
    // Find the prompt and update its folder path
    int index = findPromptIndex(itemId);
    if (index >= 0) {
        prompts[index].folderPath = newFolderPath;
        prompts[index].modified = QDateTime::currentDateTime();
        
        // Save the changes immediately
        savePrompts();
        
        // Update the UI if this is the current prompt
        if (currentPromptId == itemId) {
            updatePromptList();
        }
        
        statusBar()->showMessage("Prompt moved to " + newFolderPath);
    }
}

void MainWindow::newPrompt()
{
    if (hasUnsavedChanges() && !currentPromptId.isEmpty()) {
        if (!promptBeforeSwitch())
            return;
    }

    promptListView->clearSelection();

    currentPromptId.clear();
    originalPrompt = Prompt();
    isNewPrompt = true;
    hasChanges = false;

    titleEdit->clear();
    bodyEdit->clear();

    updateUI();
    titleEdit->setFocus();
    statusBar()->showMessage("New prompt - enter title and body");
}

void MainWindow::newFolder()
{
    QModelIndex parentIndex = getCurrentFolderIndex();
    
    // If a prompt is selected, use its parent folder instead
    if (parentIndex.isValid()) {
        QModelIndex sourceIndex = folderProxyModel->mapToSource(parentIndex);
        if (sourceIndex.isValid()) {
            FolderTreeItem *item = static_cast<FolderTreeItem*>(sourceIndex.internalPointer());
            if (item && item->type() == FolderTreeItem::PromptType) {
                parentIndex = parentIndex.parent();
            }
        }
    }
    
    if (!parentIndex.isValid()) {
        parentIndex = QModelIndex();
    }

    bool ok;
    QString name = QInputDialog::getText(this, "New Folder",
                                         "Enter folder name:", QLineEdit::Normal,
                                         "", &ok);
    if (ok && !name.isEmpty()) {
        QModelIndex sourceParent = folderProxyModel->mapToSource(parentIndex);
        folderModel->insertFolder(sourceParent, name);
        folderTreeView->expand(parentIndex);
        statusBar()->showMessage(QString("Created folder: %1").arg(name));
    }
}

void MainWindow::savePrompt()
{
    QString title = titleEdit->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Title cannot be empty.");
        titleEdit->setFocus();
        return;
    }

    QString body = bodyEdit->toPlainText();
    QString folderPath = getCurrentFolderPath();

    if (isNewPrompt) {
        Prompt prompt;
        prompt.id = generateId();
        prompt.title = title;
        prompt.body = body;
        prompt.folderPath = folderPath;
        prompt.created = QDateTime::currentDateTime();
        prompt.modified = QDateTime::currentDateTime();

        prompts.append(prompt);

        QModelIndex folderIndex = getCurrentFolderIndex();
        
        // If a prompt is selected, use its parent folder instead
        QModelIndex sourceIndex = folderProxyModel->mapToSource(folderIndex);
        if (sourceIndex.isValid()) {
            FolderTreeItem *item = static_cast<FolderTreeItem*>(sourceIndex.internalPointer());
            if (item && item->type() == FolderTreeItem::PromptType) {
                sourceIndex = sourceIndex.parent();
            }
        }
        
        folderModel->insertPrompt(sourceIndex, title, prompt.id);

        currentPromptId = prompt.id;
        originalPrompt = prompt;
        isNewPrompt = false;
        hasChanges = false;

        updatePromptList();
        statusBar()->showMessage("New prompt created");
    } else {
        int index = findPromptIndex(currentPromptId);
        if (index == -1) {
            QMessageBox::warning(this, "Error", "Prompt not found.");
            return;
        }

        Prompt &prompt = prompts[index];
        prompt.title = title;
        prompt.body = body;
        prompt.folderPath = folderPath;
        prompt.modified = QDateTime::currentDateTime();

        // Update folder model if needed
        QModelIndex itemIndex = folderModel->findItemById(currentPromptId);
        if (itemIndex.isValid()) {
            folderModel->setData(itemIndex, title, Qt::EditRole);
        }

        originalPrompt = prompt;
        hasChanges = false;

        updatePromptList();
        statusBar()->showMessage("Prompt updated");
    }

    savePrompts();
    updateUI();
}
// Helper method to count items in a folder
void MainWindow::countItemsInFolder(FolderTreeItem *folder, int &promptCount, int &folderCount)
{
    for (int i = 0; i < folder->childCount(); ++i) {
        FolderTreeItem *child = folder->child(i);
        if (!child) continue;  // Safety check
        if (child->type() == FolderTreeItem::FolderType) {
            folderCount++;
            countItemsInFolder(child, promptCount, folderCount);
        } else if (child->type() == FolderTreeItem::PromptType) {
            promptCount++;
        }
    }
}

// Helper method to delete a folder and all its contents
void MainWindow::deleteFolderAndContents(FolderTreeItem *folder)
{
    // First delete all child prompts from our data
    deletePromptsInFolder(folder);

    // Then remove the folder from the model
    QModelIndex folderIndex = folderModel->findItemById(folder->id());
    if (folderIndex.isValid()) {
        folderModel->removeItem(folderIndex);
    }

    savePrompts();
}

// Helper method to delete all prompts in a folder (recursive)
void MainWindow::deletePromptsInFolder(FolderTreeItem *folder)
{
    for (int i = 0; i < folder->childCount(); ++i) {
        FolderTreeItem *child = folder->child(i);
        if (!child) continue;  // Safety check
        if (child->type() == FolderTreeItem::FolderType) {
            deletePromptsInFolder(child);
        } else if (child->type() == FolderTreeItem::PromptType) {
            // Remove prompt from our data vector
            QString promptId = child->id();
            int index = findPromptIndex(promptId);
            if (index >= 0) {
                prompts.removeAt(index);
            }
        }
    }
}
void MainWindow::deleteItem()
{
    // Check if we have a folder selected
    QModelIndex currentFolderIndex = folderTreeView->currentIndex();
    if (currentFolderIndex.isValid()) {
        QModelIndex sourceIndex = folderProxyModel->mapToSource(currentFolderIndex);
        FolderTreeItem *item = folderModel->getItem(sourceIndex);

        if (item && item->type() == FolderTreeItem::FolderType) {
            // Don't allow deleting the root folder
            if (item == folderModel->getItem(QModelIndex())) {
                QMessageBox::warning(this, "Cannot Delete", "Cannot delete the root folder.");
                return;
            }

            // Count items in the folder
            int promptCount = 0;
            int folderCount = 0;
            countItemsInFolder(item, promptCount, folderCount);

            QString message;
            if (promptCount > 0 && folderCount > 0) {
                message = QString("Are you sure you want to delete folder '%1'?\n\n"
                                 "This folder contains:\n"
                                 "- %2 prompts\n"
                                 "- %3 subfolders\n\n"
                                 "All content will be permanently deleted.")
                                 .arg(item->data(0).toString())
                                 .arg(promptCount)
                                 .arg(folderCount);
            } else if (promptCount > 0) {
                message = QString("Are you sure you want to delete folder '%1'?\n\n"
                                 "This folder contains %2 prompts that will be permanently deleted.")
                                 .arg(item->data(0).toString())
                                 .arg(promptCount);
            } else if (folderCount > 0) {
                message = QString("Are you sure you want to delete folder '%1'?\n\n"
                                 "This folder contains %2 subfolders that will be permanently deleted.")
                                 .arg(item->data(0).toString())
                                 .arg(folderCount);
            } else {
                message = QString("Are you sure you want to delete the empty folder '%1'?")
                                 .arg(item->data(0).toString());
            }

            int result = QMessageBox::question(this, "Confirm Delete",
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (result == QMessageBox::Yes) {
                // Delete folder and all its contents
                deleteFolderAndContents(item);

                // Clear selection if we deleted the currently selected prompt
                if (!currentPromptId.isEmpty()) {
                    // Check if the deleted folder contained our current prompt
                    QString currentFolderPath = getCurrentFolderPath();
                    int promptIndex = findPromptIndex(currentPromptId);
                    if (promptIndex >= 0 && prompts[promptIndex].folderPath.startsWith(item->data(0).toString())) {
                        currentPromptId.clear();
                        titleEdit->clear();
                        bodyEdit->clear();
                    }
                }

                updatePromptList();
                updateUI();
                statusBar()->showMessage("Folder deleted");
            }
            return;
        }
    }

    // Original prompt deletion code (keep this for deleting prompts)
    if (currentPromptId.isEmpty()) {
        return;
    }

    int result = QMessageBox::question(this, "Confirm Delete",
        "Are you sure you want to delete this prompt?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    int index = findPromptIndex(currentPromptId);
    if (index == -1) {
        QMessageBox::warning(this, "Error", "Prompt not found.");
        return;
    }

    prompts.removeAt(index);

    QModelIndex itemIndex = folderModel->findItemById(currentPromptId);
    if (itemIndex.isValid()) {
        folderModel->removeItem(itemIndex);
    }

    promptListView->clearSelection();
    currentPromptId.clear();
    originalPrompt = Prompt();
    isNewPrompt = false;
    hasChanges = false;

    savePrompts();
    updatePromptList();
    updateUI();
    statusBar()->showMessage("Prompt deleted");
}

void MainWindow::copyToClipboard()
{
    if (currentPromptId.isEmpty()) {
        return;
    }
    copyTextToClipboard(bodyEdit->toPlainText(), "Copied to clipboard");
}

void MainWindow::copyTextToClipboard(const QString &text, const QString &statusLabel)
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text, QClipboard::Clipboard);
    if (clipboard->supportsSelection()) {
        clipboard->setText(text, QClipboard::Selection);
    }
    statusBar()->showMessage(statusLabel);
}

void MainWindow::showFolderTreeContextMenu(const QPoint &pos)
{
    const QModelIndex proxyIndex = folderTreeView->indexAt(pos);
    if (!proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = folderProxyModel->mapToSource(proxyIndex);
    FolderTreeItem *item = sourceIndex.isValid()
        ? static_cast<FolderTreeItem*>(sourceIndex.internalPointer())
        : nullptr;
    if (!item) {
        return;
    }

    const QString folderPath = folderPathForIndex(proxyIndex);
    QMenu menu(this);

    if (item->type() == FolderTreeItem::PromptType) {
        // Prompts are addressed by id over the API; the path is its folder.
        const QString promptId = item->id();
        QAction *copyId = menu.addAction("Copy Prompt ID");
        connect(copyId, &QAction::triggered, this, [this, promptId]() {
            copyTextToClipboard(promptId, "Copied prompt ID");
        });
        // Include the prompt's own title so the path points at the prompt,
        // e.g. Work/Email/Invoice reminder (folderPath alone is its folder).
        const QString promptTitle = item->data(0).toString();
        const QString fullPath = folderPath.isEmpty()
            ? promptTitle
            : folderPath + '/' + promptTitle;
        QAction *copyPath = menu.addAction("Copy Prompt Path");
        connect(copyPath, &QAction::triggered, this, [this, fullPath]() {
            copyTextToClipboard(fullPath, "Copied prompt path");
        });
    } else {
        // Folders are addressed by their path (A/B/C) over the API.
        QAction *copyPath = menu.addAction("Copy Folder Path");
        connect(copyPath, &QAction::triggered, this, [this, folderPath]() {
            copyTextToClipboard(folderPath, "Copied folder path");
        });
    }

    menu.exec(folderTreeView->viewport()->mapToGlobal(pos));
}

void MainWindow::updateUI()
{
    bool hasSelection = !currentPromptId.isEmpty();
    bool isEditingNewPrompt = isNewPrompt;

    titleEdit->setEnabled(hasSelection || isEditingNewPrompt);
    bodyEdit->setEnabled(hasSelection || isEditingNewPrompt);

    if (hasSelection || isEditingNewPrompt) {
        titleEdit->setStyleSheet("QLineEdit { background-color: white; color: black; }");
        bodyEdit->setStyleSheet("QPlainTextEdit { background-color: white; color: black; }");
    } else {
        titleEdit->setStyleSheet("QLineEdit { background-color: #f8f8f8; color: #666666; }");
        bodyEdit->setStyleSheet("QPlainTextEdit { background-color: #f8f8f8; color: #666666; }");
    }

    noPromptLabel->setVisible(!hasSelection && !isEditingNewPrompt);

    bool canSave = false;
    if (isEditingNewPrompt) {
        canSave = !titleEdit->text().trimmed().isEmpty() && hasChanges;
    } else if (hasSelection) {
        canSave = hasChanges && !titleEdit->text().trimmed().isEmpty();
    }

    saveButton->setEnabled(canSave);
    deleteButton->setEnabled(hasSelection && !isEditingNewPrompt);
    copyButton->setEnabled(hasSelection);

    if (hasSelection) {
        QString title = titleEdit->text().isEmpty() ? "Untitled" : titleEdit->text();
        if (hasChanges) {
            setWindowTitle(QString("%0* - Prompt Manager").arg(title));
        } else {
            setWindowTitle(QString("%0 - Prompt Manager").arg(title));
        }
    } else if (isEditingNewPrompt) {
        if (hasChanges && !titleEdit->text().trimmed().isEmpty()) {
            setWindowTitle("New Prompt* - Prompt Manager");
        } else {
            setWindowTitle("New Prompt - Prompt Manager");
        }
    } else {
        setWindowTitle("Prompt Manager");
    }

    titleEdit->update();
    bodyEdit->update();
}

void MainWindow::markUnsavedChanges()
{
    if (!isNewPrompt && !currentPromptId.isEmpty()) {
        hasChanges = true;
        updateUI();
    }
}

void MainWindow::diagnosticCheck()
{
    QString report = "=== Diagnostic Report ===\n\n";
    
    // Check data consistency
    report += QString("Total prompts in data: %1\n").arg(prompts.size());
    
    // Check folder tree
    report += "\nFolder Tree Structure:\n";
    QStringList treeInfo;
    diagnosticTraverseTree(QModelIndex(), "", treeInfo);
    report += treeInfo.join("\n");
    
    // Check for orphaned prompts
    report += "\n\nOrphaned Items Check:\n";
    QSet<QString> treePromptIds;
    collectTreePromptIds(QModelIndex(), treePromptIds);
    
    int orphanedCount = 0;
    for (const Prompt &prompt : prompts) {
        if (!treePromptIds.contains(prompt.id)) {
            report += QString("- Orphaned prompt: '%1' (ID: %2)\n").arg(prompt.title).arg(prompt.id);
            orphanedCount++;
        }
    }
    
    if (orphanedCount == 0) {
        report += "No orphaned prompts found.\n";
    }
    
    // Check for duplicate IDs
    report += "\nDuplicate ID Check:\n";
    QMap<QString, int> idCount;
    for (const Prompt &prompt : prompts) {
        idCount[prompt.id]++;
    }
    
    int duplicates = 0;
    for (auto it = idCount.begin(); it != idCount.end(); ++it) {
        if (it.value() > 1) {
            report += QString("- ID '%1' appears %2 times\n").arg(it.key()).arg(it.value());
            duplicates++;
        }
    }
    
    if (duplicates == 0) {
        report += "No duplicate IDs found.\n";
    }
    
    // Check selected item
    QModelIndex currentIndex = folderTreeView->currentIndex();
    if (currentIndex.isValid()) {
        QModelIndex sourceIndex = folderProxyModel->mapToSource(currentIndex);
        FolderTreeItem *item = folderModel->getItem(sourceIndex);
        if (item) {
            report += QString("\nCurrently Selected Item:\n");
            report += QString("- Type: %1\n").arg(item->type() == FolderTreeItem::FolderType ? "Folder" : "Prompt");
            report += QString("- Name: %1\n").arg(item->data(0).toString());
            report += QString("- ID: %1\n").arg(item->id());
            
            // Check if it's a prompt that thinks it's a folder
            if (item->type() == FolderTreeItem::FolderType) {
                int promptIndex = findPromptIndex(item->id());
                if (promptIndex >= 0) {
                    report += QString("- WARNING: This folder has an ID matching prompt '%1'!\n").arg(prompts[promptIndex].title);
                }
            }
        }
    }
    
    // Display report
    QDialog dialog(this);
    dialog.setWindowTitle("Diagnostic Report");
    dialog.resize(600, 500);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QTextEdit *textEdit = new QTextEdit;
    textEdit->setPlainText(report);
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);
    
    QPushButton *closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    dialog.exec();
}

void MainWindow::diagnosticTraverseTree(const QModelIndex &parent, const QString &indent, QStringList &output)
{
    if (!parent.isValid()) {
        // Start with root
        for (int i = 0; i < folderModel->rowCount(); ++i) {
            diagnosticTraverseTree(folderModel->index(i, 0), indent, output);
        }
        return;
    }
    
    FolderTreeItem *item = folderModel->getItem(parent);
    if (!item) return;
    
    QString line = indent + "- " + item->data(0).toString();
    line += QString(" [%1]").arg(item->type() == FolderTreeItem::FolderType ? "FOLDER" : "PROMPT");
    line += QString(" ID: %1").arg(item->id());
    
    output.append(line);
    
    // Recurse into children
    for (int i = 0; i < folderModel->rowCount(parent); ++i) {
        diagnosticTraverseTree(folderModel->index(i, 0, parent), indent + "  ", output);
    }
}

void MainWindow::collectTreePromptIds(const QModelIndex &parent, QSet<QString> &promptIds)
{
    if (!parent.isValid()) {
        // Start with root
        for (int i = 0; i < folderModel->rowCount(); ++i) {
            collectTreePromptIds(folderModel->index(i, 0), promptIds);
        }
        return;
    }
    
    FolderTreeItem *item = folderModel->getItem(parent);
    if (!item) return;
    
    if (item->type() == FolderTreeItem::PromptType) {
        promptIds.insert(item->id());
    }
    
    // Recurse into children
    for (int i = 0; i < folderModel->rowCount(parent); ++i) {
        collectTreePromptIds(folderModel->index(i, 0, parent), promptIds);
    }
}

// ======================= REST API integration =======================

QModelIndex MainWindow::ensureFolderPath(const QString &folderPath)
{
    QModelIndex folderIndex; // invalid == root
    if (folderPath.isEmpty())
        return folderIndex;

    const QStringList parts = folderPath.split('/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool found = false;
        for (int i = 0; i < folderModel->rowCount(folderIndex); ++i) {
            QModelIndex idx = folderModel->index(i, 0, folderIndex);
            FolderTreeItem *item = folderModel->getItem(idx);
            if (item && item->type() == FolderTreeItem::FolderType &&
                folderModel->data(idx, Qt::DisplayRole).toString() == part) {
                folderIndex = idx;
                found = true;
                break;
            }
        }
        if (!found) {
            folderModel->insertFolder(folderIndex, part);
            folderIndex = folderModel->index(folderModel->rowCount(folderIndex) - 1, 0, folderIndex);
        }
    }
    return folderIndex;
}

void MainWindow::startApiServerFromSettings()
{
    QSettings settings;
    if (!settings.value("api/enabled", false).toBool())
        return;

    QString key = settings.value("api/key").toString();
    if (key.isEmpty()) {
        key = generateApiKey();
        settings.setValue("api/key", key);
    }
    quint16 port = static_cast<quint16>(settings.value("api/port", 8770).toUInt());

    if (!apiServer)
        apiServer = new ApiServer(this, this);

    QString err;
    if (apiServer->start(port, key, &err))
        statusBar()->showMessage(QString("API server listening on http://127.0.0.1:%1").arg(port));
    else
        statusBar()->showMessage(QString("API server failed to start: %1").arg(err));
}

void MainWindow::openApiSettings()
{
    QSettings settings;
    QDialog dialog(this);
    dialog.setWindowTitle("API Server");
    dialog.resize(620, 580);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *intro = new QLabel(
        "<b>REST API</b><br>"
        "Let agents and tools read and edit your prompts with full parity to the app "
        "(create, edit, delete, search). The server listens on <code>127.0.0.1</code> "
        "(localhost only) and requires the API key below on every request.");
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QGroupBox *serverBox = new QGroupBox("Server");
    QFormLayout *form = new QFormLayout(serverBox);
    QCheckBox *enabledCheck = new QCheckBox("Enable API server");
    enabledCheck->setChecked(settings.value("api/enabled", false).toBool());
    form->addRow(enabledCheck);
    QSpinBox *portSpin = new QSpinBox;
    portSpin->setRange(1024, 65535);
    portSpin->setValue(settings.value("api/port", 8770).toInt());
    form->addRow("Port:", portSpin);
    layout->addWidget(serverBox);

    QGroupBox *keyBox = new QGroupBox("API Key");
    QVBoxLayout *keyLayout = new QVBoxLayout(keyBox);
    QHBoxLayout *keyRow = new QHBoxLayout;
    QLineEdit *keyEdit = new QLineEdit(settings.value("api/key").toString());
    keyEdit->setReadOnly(true);
    QPushButton *genButton = new QPushButton("Generate");
    QPushButton *copyKeyButton = new QPushButton("Copy");
    keyRow->addWidget(keyEdit);
    keyRow->addWidget(genButton);
    keyRow->addWidget(copyKeyButton);
    keyLayout->addLayout(keyRow);
    layout->addWidget(keyBox);

    QGroupBox *detailBox = new QGroupBox("Details && Example");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailBox);
    QPlainTextEdit *details = new QPlainTextEdit;
    details->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    details->setFont(mono);
    detailLayout->addWidget(details);
    layout->addWidget(detailBox, 1);

    auto refreshDetails = [&]() {
        if (keyEdit->text().isEmpty())
            keyEdit->setText(generateApiKey());
        const QString key = keyEdit->text();
        const int port = portSpin->value();
        const QString base = QString("http://127.0.0.1:%1/api").arg(port);
        QString text;
        text += QString("Base URL:  %1\n").arg(base);
        text += QString("Auth:      Authorization: Bearer %1\n").arg(key);
        text += QString("           (or  X-API-Key: %1)\n\n").arg(key);
        text += "Full API reference: see API.md (shipped with the app)\n\n";
        text += "Examples\n--------\n";
        text += "# List / search prompts\n";
        text += QString("curl -H \"Authorization: Bearer %1\" \\\n     \"%2/prompts?q=email\"\n\n").arg(key, base);
        text += "# Create a prompt\n";
        text += QString("curl -X POST -H \"Authorization: Bearer %1\" \\\n"
                        "     -H \"Content-Type: application/json\" \\\n"
                        "     -d '{\"title\":\"Greeting\",\"body\":\"Hello!\",\"folderPath\":\"General\"}' \\\n"
                        "     %2/prompts\n\n").arg(key, base);
        text += "# Update a prompt\n";
        text += QString("curl -X PUT -H \"Authorization: Bearer %1\" \\\n"
                        "     -H \"Content-Type: application/json\" \\\n"
                        "     -d '{\"body\":\"Updated text\"}' \\\n"
                        "     %2/prompts/<id>\n\n").arg(key, base);
        text += "# Delete a prompt\n";
        text += QString("curl -X DELETE -H \"Authorization: Bearer %1\" \\\n     %2/prompts/<id>\n").arg(key, base);
        details->setPlainText(text);
    };
    refreshDetails();

    connect(genButton, &QPushButton::clicked, &dialog, [&]() { keyEdit->setText(generateApiKey()); refreshDetails(); });
    connect(copyKeyButton, &QPushButton::clicked, &dialog, [&]() { QApplication::clipboard()->setText(keyEdit->text()); });
    connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), &dialog, [&](int) { refreshDetails(); });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QString key = keyEdit->text();
    if (key.isEmpty())
        key = generateApiKey();
    settings.setValue("api/enabled", enabledCheck->isChecked());
    settings.setValue("api/port", portSpin->value());
    settings.setValue("api/key", key);

    if (!apiServer)
        apiServer = new ApiServer(this, this);

    if (enabledCheck->isChecked()) {
        QString err;
        if (apiServer->start(static_cast<quint16>(portSpin->value()), key, &err))
            statusBar()->showMessage(QString("API server listening on http://127.0.0.1:%1").arg(portSpin->value()));
        else
            QMessageBox::warning(this, "API Server",
                QString("Could not start server on port %1:\n%2").arg(portSpin->value()).arg(err));
    } else {
        apiServer->stop();
        statusBar()->showMessage("API server stopped");
    }
}

ApiResponse MainWindow::apiListPrompts(const QString &folder, const QString &query)
{
    QJsonArray arr;
    const QString q = query.trimmed();
    for (const Prompt &p : prompts) {
        if (!folder.isEmpty() && p.folderPath != folder)
            continue;
        if (!q.isEmpty() &&
            !p.title.contains(q, Qt::CaseInsensitive) &&
            !p.body.contains(q, Qt::CaseInsensitive) &&
            !p.folderPath.contains(q, Qt::CaseInsensitive))
            continue;
        arr.append(p.toJson());
    }
    QJsonObject b;
    b["prompts"] = arr;
    b["count"] = arr.size();
    return ApiResponse::ok(b);
}

ApiResponse MainWindow::apiGetPrompt(const QString &id)
{
    int idx = findPromptIndex(id);
    if (idx < 0)
        return ApiResponse::error(404, "Prompt not found");
    return ApiResponse::ok(prompts[idx].toJson());
}

ApiResponse MainWindow::apiCreatePrompt(const QJsonObject &input)
{
    const QString title = input.value("title").toString().trimmed();
    if (title.isEmpty())
        return ApiResponse::error(400, "Field 'title' is required");

    Prompt p;
    p.id = generateId();
    p.title = title;
    p.body = input.value("body").toString();
    QString folderPath = input.value("folderPath").toString().trimmed();
    if (folderPath.isEmpty())
        folderPath = "General";
    p.folderPath = folderPath;
    p.created = QDateTime::currentDateTime();
    p.modified = p.created;

    prompts.append(p);
    QModelIndex folderIndex = ensureFolderPath(folderPath);
    folderModel->insertPrompt(folderIndex, p.title, p.id);

    savePrompts();
    updatePromptList();
    updateUI();

    return ApiResponse::created(p.toJson());
}

ApiResponse MainWindow::apiUpdatePrompt(const QString &id, const QJsonObject &input)
{
    int idx = findPromptIndex(id);
    if (idx < 0)
        return ApiResponse::error(404, "Prompt not found");

    Prompt &p = prompts[idx];
    bool folderChanged = false;

    if (input.contains("title")) {
        const QString t = input.value("title").toString().trimmed();
        if (t.isEmpty())
            return ApiResponse::error(400, "Field 'title' cannot be empty");
        p.title = t;
    }
    if (input.contains("body"))
        p.body = input.value("body").toString();
    if (input.contains("folderPath")) {
        QString f = input.value("folderPath").toString().trimmed();
        if (f.isEmpty())
            f = "General";
        if (f != p.folderPath) {
            p.folderPath = f;
            folderChanged = true;
        }
    }
    p.modified = QDateTime::currentDateTime();

    // Reflect changes in the tree: move the node when the folder changed,
    // otherwise just update its displayed title.
    QModelIndex node = folderModel->findItemById(id);
    if (folderChanged) {
        if (node.isValid())
            folderModel->removeItem(node);
        QModelIndex folderIndex = ensureFolderPath(p.folderPath);
        folderModel->insertPrompt(folderIndex, p.title, id);
    } else if (node.isValid()) {
        folderModel->setData(node, p.title, Qt::EditRole);
    }

    // Keep the editor in sync if this prompt is open and has no pending edits.
    if (currentPromptId == id && !hasChanges) {
        QSignalBlocker titleBlocker(titleEdit);
        QSignalBlocker bodyBlocker(bodyEdit);
        titleEdit->setText(p.title);
        bodyEdit->setPlainText(p.body);
        originalPrompt = p;
    }

    savePrompts();
    updatePromptList();
    updateUI();

    return ApiResponse::ok(p.toJson());
}

ApiResponse MainWindow::apiDeletePrompt(const QString &id)
{
    int idx = findPromptIndex(id);
    if (idx < 0)
        return ApiResponse::error(404, "Prompt not found");

    prompts.removeAt(idx);

    QModelIndex node = folderModel->findItemById(id);
    if (node.isValid())
        folderModel->removeItem(node);

    // Clear the editor if the deleted prompt was open.
    if (currentPromptId == id) {
        currentPromptId.clear();
        QSignalBlocker titleBlocker(titleEdit);
        QSignalBlocker bodyBlocker(bodyEdit);
        titleEdit->clear();
        bodyEdit->clear();
        hasChanges = false;
    }

    savePrompts();
    updatePromptList();
    updateUI();

    QJsonObject b;
    b["deleted"] = id;
    return ApiResponse::ok(b);
}

static void collectFolderPaths(FolderTreeModel *model, const QModelIndex &parent,
                               const QString &prefix, QJsonArray &out)
{
    for (int i = 0; i < model->rowCount(parent); ++i) {
        QModelIndex idx = model->index(i, 0, parent);
        FolderTreeItem *item = model->getItem(idx);
        if (!item || item->type() != FolderTreeItem::FolderType)
            continue;
        const QString name = item->data(0).toString();
        const QString path = prefix.isEmpty() ? name : prefix + "/" + name;
        QJsonObject o;
        o["path"] = path;
        o["name"] = name;
        out.append(o);
        collectFolderPaths(model, idx, path, out);
    }
}

ApiResponse MainWindow::apiListFolders()
{
    QJsonArray arr;
    collectFolderPaths(folderModel, QModelIndex(), QString(), arr);
    QJsonObject b;
    b["folders"] = arr;
    b["count"] = arr.size();
    return ApiResponse::ok(b);
}

ApiResponse MainWindow::apiCreateFolder(const QJsonObject &input)
{
    const QString path = input.value("path").toString().trimmed();
    if (path.isEmpty())
        return ApiResponse::error(400, "Field 'path' is required");

    ensureFolderPath(path);
    folderTreeView->expandAll();
    updateUI();

    QJsonObject b;
    b["path"] = path;
    b["note"] = "Empty folders persist across restarts only once they contain a prompt.";
    return ApiResponse::created(b);
}

ApiResponse MainWindow::apiDeleteFolder(const QString &path)
{
    const QString target = path.trimmed();
    if (target.isEmpty())
        return ApiResponse::error(400, "Query parameter 'path' is required");

    // Walk to the folder node without creating anything.
    QModelIndex folderIndex; // root
    const QStringList parts = target.split('/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool found = false;
        for (int i = 0; i < folderModel->rowCount(folderIndex); ++i) {
            QModelIndex idx = folderModel->index(i, 0, folderIndex);
            FolderTreeItem *item = folderModel->getItem(idx);
            if (item && item->type() == FolderTreeItem::FolderType &&
                folderModel->data(idx, Qt::DisplayRole).toString() == part) {
                folderIndex = idx;
                found = true;
                break;
            }
        }
        if (!found)
            return ApiResponse::error(404, "Folder not found");
    }
    if (!folderIndex.isValid())
        return ApiResponse::error(400, "Cannot delete the root");

    FolderTreeItem *folder = folderModel->getItem(folderIndex);
    if (!folder || folder->type() != FolderTreeItem::FolderType)
        return ApiResponse::error(404, "Folder not found");

    // Remove all prompts contained (recursively) from the data store first.
    const QVector<FolderTreeItem *> contained = folder->getAllPrompts();
    int removed = 0;
    for (FolderTreeItem *pItem : contained) {
        int i = findPromptIndex(pItem->id());
        if (i >= 0) {
            prompts.removeAt(i);
            ++removed;
        }
        if (currentPromptId == pItem->id()) {
            currentPromptId.clear();
            QSignalBlocker titleBlocker(titleEdit);
            QSignalBlocker bodyBlocker(bodyEdit);
            titleEdit->clear();
            bodyEdit->clear();
            hasChanges = false;
        }
    }
    folderModel->removeItem(folderIndex);

    savePrompts();
    updatePromptList();
    updateUI();

    QJsonObject b;
    b["deleted"] = target;
    b["promptsRemoved"] = removed;
    return ApiResponse::ok(b);
}
