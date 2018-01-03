#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#include <fstream>
#include <net/libwsIO.h>
#include "megachatapi.h"
//#include <asyncTools.h>

using namespace std;
using namespace promise;
using namespace mega;
using namespace megachat;
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
    void onEsidLogout();
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

extern "C" void myMegaPostMessageToGui(void* msg, void* appCtx)
{
    QEvent* event = new GcmEvent(msg);
    QApplication::postEvent(&appDelegate, event);
}

using namespace strophe;

void setVidencParams();
void saveSid(const char* sdkSid);

void sigintHandler(int)
{
    printf("SIGINT Received\n"); //don't use the logger, as it may cause a deadlock
    fflush(stdout);
    marshallCall([]{mainWin->close();}, NULL);
}

std::string gAppDir = karere::createAppDir();
std::unique_ptr<WebsocketsIO> gWebsocketsIO;
std::unique_ptr<karere::Client> gClient;
std::unique_ptr<::mega::MegaApi> gSdk;
std::unique_ptr<::megachat::MegaChatApi> gMegaChatApi;


void createWindowAndClient()
{
    mainWin = new MainWindow();
    gSdk.reset(new ::mega::MegaApi("karere-native", gAppDir.c_str(), "Karere Native"));
    gSdk->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
    gSdk->addListener(mainWin);
    gSdk->addRequestListener(mainWin);

    gMegaChatApi.reset(new ::megachat::MegaChatApi(gSdk.get()));
    gMegaChatApi->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    gMegaChatApi->addChatRequestListener(mainWin);
    gMegaChatApi->addChatListener(mainWin);

    #ifndef KARERE_DISABLE_WEBRTC
        gMegaChatApi->addChatCallListener(mainWin);
    #endif

    //Read sid if exists
    char buf[256];
    const char* sid = nullptr;
    std::ifstream sidf(gAppDir+"/sid");
    if (!sidf.fail())
    {
        sidf.getline(buf, 256);
        if (!sidf.fail())
            sid = buf;
    }
    sidf.close();
    int initializationState = gMegaChatApi->init(sid);

    //Both definitions for login delegate
    karere::IApp::ILoginDialog* mLoginDlg= NULL;
    //IApp::ILoginDialog::Handle mLoginDlg= NULL;
    if (!sid)
    {
        assert(initializationState == MegaChatApi::INIT_WAITING_NEW_SESSION);

        //LOOP FOR LOGIN METHOD (PENDING)
        //Two methods to create a login dialog, the first one doesn't have persistence
        mLoginDlg=mainWin->createLoginDialog();
        //mLoginDlg->assign(mainWin->createLoginDialog());

        auto pms = mLoginDlg->requestCredentials();  //Crash in this line
        pms
            .then([&mLoginDlg](const std::pair<std::string, std::string>& cred)
            {
                if(mLoginDlg)
                {
                     mLoginDlg->setState(IApp::ILoginDialog::kLoggingIn);
                }

                return gClient->api.callIgnoreResult(&mega::MegaApi::login, cred.first.c_str(), cred.second.c_str());
            })

            .fail([](const promise::Error& err)
            {
            });
    }
    else
    {
        assert(initializationState == MegaChatApi::INIT_OFFLINE_SESSION);
    }

    gWebsocketsIO.reset(new LibwsIO());
    gClient.reset(new karere::Client(*gSdk, gWebsocketsIO.get(), *mainWin, gAppDir, 0));
    mainWin->setClient(*gClient);
    QObject::connect(mainWin, SIGNAL(esidLogout()), &appDelegate, SLOT(onEsidLogout()));
}


int main(int argc, char **argv)
{
    karere::globalInit(myMegaPostMessageToGui, 0, (gAppDir+"/log.txt").c_str(), 500);
    const char* customApiUrl = getenv("KR_API_URL");
    if (customApiUrl)
    {
        KR_LOG_WARNING("Using custom API server, due to KR_API_URL env variable");
        ::mega::MegaClient::APIURL = customApiUrl;
    }
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
    createWindowAndClient();
    return a.exec();
}
void setVidencParams()
{
#ifndef KARERE_DISABLE_WEBRTC
    const char* val;
    auto& rtc = *gClient->rtc;
    if ((val = getenv("KR_VIDENC_MAXH")))
    {
        rtc.setMediaConstraint("maxHeight", val);
    }
    if ((val = getenv("KR_VIDENC_MAXW")))
    {
        rtc.setMediaConstraint("maxWidth", val);
    }

    if ((val = getenv("KR_VIDENC_MAXBR")))
    {
        rtc.vidEncParams.maxBitrate = atoi(val);
    }
    if ((val = getenv("KR_VIDENC_MINBR")))
    {
        rtc.vidEncParams.minBitrate = atoi(val);
    }
    if ((val = getenv("KR_VIDENC_MAXQNT")))
    {
        rtc.vidEncParams.maxQuant = atoi(val);
    }
    if ((val = getenv("KR_VIDENC_BUFLAT")))
    {
        rtc.vidEncParams.bufLatency = atoi(val);
    }
#endif
}

void AppDelegate::onAppTerminate()
{
    static bool called = false;
    if (called)
        return;
    called = true;
    gClient->terminate();

    gSdk->localLogout(nullptr);

    marshallCall([]() //post destruction asynchronously so that all pending messages get processed before that
    {
        gClient.reset();
        gSdk.reset();
        qApp->quit(); //stop processing marshalled call messages
        gWebsocketsIO.reset();
        karere::globalCleanup();
    }, NULL);
}

void saveSid(const char* sdkSid)
{
    std::ofstream osidf(gAppDir+"/sid");
    assert(sdkSid);
    osidf << sdkSid;
    osidf.close();
}

void AppDelegate::onEsidLogout()
{
    gClient->terminate(true);

    marshallCall([this]() //post destruction asynchronously so that all pending messages get processed before that
    {
        QObject::disconnect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

        gClient.reset();
        remove((gAppDir+"/sid").c_str());
        delete mainWin;
        QMessageBox::critical(nullptr, tr("Logout"), tr("Your session has been closed remotely"));
        createWindowAndClient();

        gClient->loginSdkAndInit(nullptr)
        .then([]()
        {
            KR_LOG_DEBUG("New client initialized with new session");
            saveSid(gSdk->dumpSession());
            QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));
            gClient->connect(Presence::kInvalid);
        })
        .fail([](const promise::Error& err)
        {
            KR_LOG_ERROR("Error re-creating or logging in chat client after ESID: ", err.what());
        });
    }, NULL);
}

#include <main.moc>
