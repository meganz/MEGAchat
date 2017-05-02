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

extern "C" void myMegaPostMessageToGui(void* msg)
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
    marshallCall([]{appDelegate.onAppTerminate();});
}

std::string gAppDir = karere::createAppDir();
std::unique_ptr<karere::Client> gClient;
std::unique_ptr<::mega::MegaApi> gSdk;

void createWindowAndClient()
{
    mainWin = new MainWindow();
    gSdk.reset(new ::mega::MegaApi("karere-native", gAppDir.c_str(), "Karere Native"));
    gClient.reset(new karere::Client(*gSdk, *mainWin, gAppDir, 0));
    mainWin->setClient(*gClient);
    QObject::connect(mainWin, SIGNAL(esidLogout()), &appDelegate, SLOT(onEsidLogout()));
}

int main(int argc, char **argv)
{
    karere::globalInit(myMegaPostMessageToGui, 0, (gAppDir+"/log.txt").c_str(), 500);
    const char* staging = getenv("KR_USE_STAGING");
    if (staging && strcmp(staging, "1") == 0)
    {
        KR_LOG_WARNING("Using staging API server, due to KR_USE_STAGING env variable");
        ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";
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
    gClient->loginSdkAndInit(sid)
    .then([sid]()
    {
        if (!sid)
        {
            KR_LOG_DEBUG("Client initialized with new session");
            saveSid(gSdk->dumpSession());
        }
        else
        {
            KR_LOG_DEBUG("Client initialized");
        }
        setVidencParams();
        signal(SIGINT, sigintHandler);
        QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));
        gClient->connect(Presence::kInvalid);
    })
    .fail([](const promise::Error& err)
    {
        if (err.type() != 0 || err.code() != 0)
        {
            QMessageBox::critical(nullptr, "rtctestapp", QString::fromLatin1("Client startup failed with error:\n")+QString::fromStdString(err.msg()));
        }
        marshallCall([]()
        {
            appDelegate.onAppTerminate();
        });
    });
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
    assert(!called);
    called = true;
    gClient->terminate()
    .then([this]()
    {
        return gSdk->localLogout(nullptr);
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error logging out the Mega client: ", err.what());
    })
    .then([this]()
    {
        marshallCall([]() //post destruction asynchronously so that all pending messages get processed before that
        {
            qApp->quit(); //stop processing marshalled call messages
            gClient.reset();
            karere::globalCleanup();
        });
    });
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
    gClient->terminate(true)
    .then([this]()
    {
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
        });
    });
}

#include <main.moc>
