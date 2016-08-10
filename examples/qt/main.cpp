#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mstrophepp.h>
#include <QApplication>
#include <QDir>
#include "mainwindow.h"
#include "chatWindow.h"
#include <base/gcm.h>
#include <base/services.h>
#include <chatClient.h>
#include <sdkApi.h>
#include <chatd.h>
#include <mega/megaclient.h>
#include <karereCommon.h>
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
namespace karere
{
    APP_ALWAYS_EXPORT std::string getAppDir() { return karere::createAppDir(); }
}
std::unique_ptr<karere::Client> gClient;
std::unique_ptr<::mega::MegaApi> gSdk;
int main(int argc, char **argv)
{
    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";
//    gLogger.addUserLogger("karere-remote", new RemoteLogger);

#ifdef __APPLE__
//Set qt plugin dir for release builds
#ifdef NDEBUG
    QDir dir(argv[0]);
    #ifdef __APPLE__
        dir.cdUp();
        dir.cdUp();
        dir.cd("Plugins");
    #else
        dir.cdUp();
        dir.cd("QtPlugins");
    #endif
    QApplication::setLibraryPaths(QStringList(dir.absolutePath()));
#endif
#endif
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    services_init(myMegaPostMessageToGui, SVC_STROPHE_LOG);
    mainWin = new MainWindow();
    gSdk.reset(new ::mega::MegaApi("karere-native", karere::getAppDir().c_str(), "Karere Native"));
    gClient.reset(new karere::Client(*gSdk, *mainWin, karere::Presence::kOnline));
    mainWin->setClient(*gClient);
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

    gClient->initWithSdk()
    .then([]()
    {
        KR_LOG_DEBUG("Client initialized");
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
