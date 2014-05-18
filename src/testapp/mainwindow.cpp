#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"
#include <QThread>
#include <string>

extern MainWindow* mainWin;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotKarereEvent(void* msg)
{
    QMessageBox::information(this, "", ("signal received, thread = "+std::to_string(QThread::currentThreadId())+"\nvalue = "+(const char*)msg).c_str());
}
struct MyThread: public QThread
{
    virtual void run()
    {
        QMetaObject::invokeMethod(mainWin,
          "slotKarereEvent", Qt::QueuedConnection,
          Q_ARG(void*, (void*)"this is a test message"));
    }
};

void MainWindow::buttonPushed()
{
    QMessageBox::information(this, "",
        ("button pushed, thread = "+std::to_string(QThread::currentThreadId())).c_str());
    MyThread thread;
    thread.run();
}
