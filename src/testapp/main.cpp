#include "webrtc/base/ssladapter.h"

#include <QtGui/QApplication>
#include "mainwindow.h"

MainWindow* mainWin = NULL;

namespace mega
{
    void postMessageToGui(void* msg)
    {
        QMetaObject::invokeMethod(mainWin,
            "megaMessageSlot", Qt::QueuedConnection, Q_ARG(void*, msg));
    }
}

int main(int argc, char *argv[])
{
    rtc::InitializeSSL();
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    
    return a.exec();
}
