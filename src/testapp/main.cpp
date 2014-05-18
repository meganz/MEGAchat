#include <QtGui/QApplication>
#include "mainwindow.h"

MainWindow* mainWin = NULL;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    
    return a.exec();
}
