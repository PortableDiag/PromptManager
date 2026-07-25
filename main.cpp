#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

static QIcon applicationIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QString(":/icons/app-%1.png").arg(size), QSize(size, size));
    return icon;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Prompt Manager");
    app.setOrganizationName("PromptManager");
    app.setApplicationVersion("2.3.0");
    app.setDesktopFileName("promptmanager");
    app.setWindowIcon(applicationIcon());

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
