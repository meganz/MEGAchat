#include "webrtc/base/ssladapter.h"

#include <QtGui/QApplication>
#include "mainwindow.h"
#include "../base/gcmpp.h"

MainWindow* mainWin = NULL;

MEGA_GCM_EXPORT void megaPostMessageToGui(void* msg)
{
        QMetaObject::invokeMethod(mainWin,
            "megaMessageSlot", Qt::QueuedConnection, Q_ARG(void*, msg));
}

int main(int argc, char *argv[])
{
    rtc::InitializeSSL();
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    
    return a.exec();
}
