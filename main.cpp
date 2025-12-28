#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Prompt Snippet Manager");
    app.setOrganizationName("PromptSnippet");
    app.setApplicationVersion("1.0.0");

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
