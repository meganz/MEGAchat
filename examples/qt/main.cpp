#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mstrophepp.h>
#include <QApplication>
#include "mainwindow.h"
#include "chatWindow.h"
#include <base/gcm.h>
#include <base/services.h>
#include <chatClient.h>
#include <rtcModule/lib.h>
#include <sdkApi.h>
#include <chatd.h>
#include <mega/megaclient.h>

using namespace std;
using namespace promise;
using namespace mega;
using namespace karere;

MainWindow* mainWin = NULL;

struct GcmEvent: public QEvent
{
    static const QEvent::Type type;
    void* ptr;
    GcmEvent(void* aPtr): QEvent(type), ptr(aPtr){}
};
const QEvent::Type GcmEvent::type = (QEvent::Type)QEvent::registerEventType();

class AppDelegate: public QObject
{
    Q_OBJECT
public slots:
    void onAppTerminate();
public:
    virtual bool event(QEvent* event)
    {
        if (event->type() != GcmEvent::type)
            return false;

        megaProcessMessage(static_cast<GcmEvent*>(event)->ptr);
        return true;
    }
};

AppDelegate appDelegate;

extern "C" void myMegaPostMessageToGui(void* msg)
{
    QEvent* event = new GcmEvent(msg);
    QApplication::postEvent(&appDelegate, event);
}

using namespace strophe;


void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
    marshallCall([]{appDelegate.onAppTerminate();});

}

int main(int argc, char **argv)
{
//    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";
    gLogger.addUserLogger("karere-remote", new RemoteLogger);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    services_init(myMegaPostMessageToGui, SVC_STROPHE_LOG);
    mainWin = new MainWindow();
    gClient.reset(new karere::Client(*mainWin));
    mainWin->setClient(*gClient);
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

    gClient->init()
    .then([](int)
    {
        KR_LOG_DEBUG("Client initialized");
        vector<string> audio;
        gClient->rtc->getAudioInDevices(audio);
        for (auto& name: audio)
            mainWin->ui.audioInCombo->addItem(name.c_str());
        vector<string> video;
        gClient->rtc->getVideoInDevices(video);
        for (auto& name: video)
            mainWin->ui.videoInCombo->addItem(name.c_str());
    })
    .fail([](const promise::Error& error)
    {
        QMessageBox::critical(mainWin, "rtctestapp", QString::fromLatin1("Client startup failed with error:\n")+QString::fromStdString(error.msg()));
        mainWin->close();
        exit(1);
    });

    signal(SIGINT, sigintHandler);
    return a.exec();
}

void AppDelegate::onAppTerminate()
{
    gClient->terminate()
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error logging out the Mega client: ", err.what());
    })
    .then([this]()
    {
        qApp->quit(); //stop processing marshalled call messages
        gClient.reset();
        rtcModule::globalCleanup();
        services_shutdown();
    });
}
#include <main.moc>
