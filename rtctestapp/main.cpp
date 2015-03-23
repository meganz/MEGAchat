#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mstrophepp.h>
#include <QtGui/QApplication>
#include "mainwindow.h"
#include <base/gcm.h>
#include <base/services.h>
#include <chatClient.h>
#include <rtcModule/lib.h>
#include <sdkApi.h>

using namespace std;
using namespace promise;
using namespace mega;

MainWindow* mainWin = NULL;
unique_ptr<karere::ChatClient> gClient;

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
    marshallCall([]{mainWin->close();});
}

const char* usermail;
const char* pass = NULL;
bool inCall = false;

int main(int argc, char **argv)
{
    /* take a jid and password on the command line */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: rtctestapp <usermail> <userpass> <peermail>\n\n");
        return 1;
    }

    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->ui->callBtn->setEnabled(false);
    mainWin->ui->callBtn->setText("Login...");
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

    services_init(myMegaPostMessageToGui, SVC_STROPHE_LOG);
    mainWin->ui->calleeInput->setText(argv[3]);
    gClient.reset(new karere::ChatClient(argv[1], argv[2]));
    gClient->registerRtcHandler(new RtcEventHandler(mainWin));
    gClient->init()
    .then([](int)
    {
        rtcModule::IPtr<rtcModule::IDeviceList> audio(gClient->rtc->getAudioInDevices());
        for (size_t i=0, len=audio->size(); i<len; i++)
            mainWin->ui->audioInCombo->addItem(audio->name(i).c_str());
        rtcModule::IPtr<rtcModule::IDeviceList> video(gClient->rtc->getVideoInDevices());
        for (size_t i=0, len=video->size(); i<len; i++)
            mainWin->ui->videoInCombo->addItem(video->name(i).c_str());
        gClient->rtc->updateIceServers(KARERE_DEFAULT_TURN_SERVERS);

        mainWin->ui->callBtn->setEnabled(true);
        mainWin->ui->callBtn->setText("Call");

        std::vector<std::string> contacts = gClient->getContactList().getContactJids();

        for(size_t i=0; i<contacts.size();i++)
        {
            mainWin->ui->contactList->addItem(new QListWidgetItem(QIcon("/images/online.png"), contacts[i].c_str()));
        }
        return 0;
    })
    .fail([](const promise::Error& error)
    {
        printf("==========Client::start() promise failed:\n%s\n", error.msg().c_str());
        return error;
    });

    signal(SIGINT, sigintHandler);
    mainWin->show();

    return a.exec();
}

void AppDelegate::onAppTerminate()
{
    printf("onAppTerminate\n");
    gClient.reset();
    rtcCleanup();
    services_shutdown();
}
#include <main.moc>
