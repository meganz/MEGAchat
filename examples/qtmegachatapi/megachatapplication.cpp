#ifdef _WIN32
    #include <winsock2.h>
#endif
#include "megachatapplication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <base/gcm.h>
#include <base/services.h>
#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QInputDialog>
#include <QDrag>
#include <QDir>
#include <QMimeData>
#include <webrtc.h>
#include <ui_mainwindow.h>
#include <ui_clistitem.h>
#include <ui_loginDialog.h>
#include <ui_settings.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include "chatWindow.h"
#include "megachatapi.h"
#include "QTMegaChatListener.h"
#include "QTMegaChatRequestListener.h"
#include "QTMegaChatRoomListener.h"
#include "QTMegaChatEvent.h"
#include "mainwindow.h"
#include "chatWindow.h"
#include <sdkApi.h>
#include <chatd.h>
#include <mega/megaclient.h>
#include <karereCommon.h>
#include <fstream>
#include <net/libwsIO.h>

using namespace std;
using namespace promise;
using namespace mega;
using namespace megachat;
using namespace karere;

namespace megachatapplication {
QChar kOnlineSymbol_InProgress(0x267a);
QChar kOnlineSymbol_Set(0x25cf);
QString kOnlineStatusBtnStyle = QStringLiteral(
    u"color: %1;"
    "border: 0px;"
    "border-radius: 2px;"
    "background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1,"
        "stop:0 rgba(100,100,100,255),"
        "stop:1 rgba(160,160,160,255));");
}


void MegaChatApplication::onAppTerminate()
{
    static bool called = false;
    if (called)
        return;
    called = true;
    //delete this;
}

MegaChatApplication::MegaChatApplication(int &argc ,char** argv) : QApplication(argc,argv)
{
     this->setQuitOnLastWindowClosed(false);
     appDir = karere::createAppDir();
     configureLogs();
     mainWin = new MainWindow();
     loginDlg = nullptr;
     megaApi = new ::mega::MegaApi("karere-native", appDir.c_str(), "Karere Native");
     megaApi->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
     megaApi->addListener(this);
     megaChatApi = new ::megachat::MegaChatApi(megaApi);
     megaChatApi->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
     megaChatApi->addChatRequestListener(this);
     megaChatApi->addChatListener(this);
     mainWin->setMegaChatApi(megaChatApi);
     mainWin->setMegaApi(megaApi);
}

MegaChatApplication::~MegaChatApplication()
{
    delete megaChatApi;
    delete megaApi;
    delete mainWin;
    loginDlg->destroy();
    delete logger;
}

void MegaChatApplication::init()
{
    int initializationState = megaChatApi->init(sid);

    if (!this->getSid())
    {
        assert(initializationState == MegaChatApi::INIT_WAITING_NEW_SESSION);
        this->login();
    }
    else
    {
        assert(initializationState == MegaChatApi::INIT_OFFLINE_SESSION);
    }
}

void MegaChatApplication::login()
{
    loginDlg = new LoginDialog(nullptr);
    auto pms = loginDlg->requestCredentials();
    pms
       .then([this](const std::pair<std::string, std::string>& cred)
       {
           assert(loginDlg);
           loginDlg->setState(LoginDialog::loggingIn);
           megaApi->login(cred.first.c_str(), cred.second.c_str());
           QObject::connect(mainWin, SIGNAL(esidLogout()), this, SLOT(onAppTerminate()));
       })
       .fail([this](const promise::Error& err)
       {
            this->quit();
       });
}

void MegaChatApplication::logout()
{
    megaApi->logout();
    megaChatApi->logout();
    //Implements differents options depending on the status
}

void MegaChatApplication::readSid()
{
    char buf[256];
    std::ifstream sidf(appDir+"/sid");
    if (!sidf.fail())
    {
       sidf.getline(buf, 256);
       if (!sidf.fail())
           sid = strdup(buf);
           //strcpy (sid,buf);
    }
}

const char *MegaChatApplication::getSid() const
{
    return sid;
}

void MegaChatApplication::sigintHandler(int)
{
    printf("SIGINT Received\n"); //don't use the logger, as it may cause a deadlock
    fflush(stdout);
    mainWin->close();;
}

void MegaChatApplication::saveSid(const char* sdkSid)
{
    std::ofstream osidf(appDir+"/sid");
    assert(sdkSid);
    osidf << sdkSid;
    osidf.close();
}

void MegaChatApplication::configureLogs()
{
    std::string logPath = appDir+"/log.txt";
    logger = new MegaLoggerApplication(logPath.c_str());

    //MegaApi::addLoggerObject(logger);
    //MegaApi::setLogToConsole(false);

    MegaChatApi::setLoggerObject(logger);
    MegaChatApi::setLogToConsole(false);
    MegaChatApi::setCatchException(false);
}


//------------------------------------------------------------------------------------------------------>
//implementation for MegaChatListener
void MegaChatApplication::onChatInitStateUpdate(megachat::MegaChatApi *api, int newState)
{
    if (!mainWin->isVisible() && (newState == karere::Client::kInitHasOfflineSession
                      || newState == karere::Client::kInitHasOnlineSession))
    {
        mainWin->show();
    }
    else if (newState == karere::Client::kInitErrSidInvalid)
    {
        mainWin->hide();
        Q_EMIT mainWin->esidLogout();
    }

    if (newState == karere::Client::kInitHasOfflineSession ||
            newState == karere::Client::kInitHasOnlineSession)
    {
        mainWin->setWindowTitle(megaChatApi->getMyEmail());
    }
    else
    {
        mainWin->setWindowTitle("");
    }
}
//------------------------------------------------------------------------------------------------------>


void MegaChatApplication::onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item){}
void MegaChatApplication::onChatConnectionStateUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, int newState) {}

void MegaChatApplication::onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress)
{
    if (api->getMyUserHandle()== userhandle)
    {
        mainWin->ui.mOnlineStatusBtn->setText(inProgress
            ?megachatapplication::kOnlineSymbol_InProgress
            :megachatapplication::kOnlineSymbol_Set);
        //ui.mOnlineStatusBtn->setStyleSheet(
        //    kOnlineStatusBtnStyle.arg(gOnlineIndColors[api->getPresenceConfig() ? status : 0]));
    }
    else
    {
            // PENDING: update the presence for contacts
    }
}

void MegaChatApplication::onChatPresenceConfigUpdate(megachat::MegaChatApi *api, megachat::MegaChatPresenceConfig *config)
{
    mainWin->ui.mOnlineStatusBtn->setText(config->isPending()
        ?megachatapplication::kOnlineSymbol_InProgress
        :megachatapplication::kOnlineSymbol_Set);
  //  ui.mOnlineStatusBtn->setStyleSheet(
   //     kOnlineStatusBtnStyle.arg(gOnlineIndColors[config->getOnlineStatus()]));
}

void MegaChatApplication::onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, MegaError *e)
{
    switch (request->getType())
    {
        case mega::MegaRequest::TYPE_LOGIN:
            if (e->getErrorCode() == mega::MegaError::API_OK)
            {
               loginDlg->setState(LoginDialog::fetchingNodes);
               api->fetchNodes();
            }
            else
            {
               loginDlg->setState(LoginDialog::badCredentials);
               //Request credentials again
            }
            break;

        case mega::MegaRequest::TYPE_FETCH_NODES:
            if (e->getErrorCode() == mega::MegaError::API_OK)
            {
               loginDlg->hide();
               megaChatApi->connect();
               mainWin->show();
            }
            else
            {
                // go back to login dialog
            }
            break;
    }
}

void MegaChatApplication::addChats()
{
    MegaChatHandle hand;


    MegaChatListItemList * chatList = megaChatApi->getChatListItems();
    for(int i=0; i<chatList->size();i++)
    {
        if (!chatList->get(i)->isGroup())
        {

             hand=chatList->get(i)->getChatId();
            mainWin->addPeerChat(chatList->get(i)->getChatId(),this->megaChatApi);
        }
    }
}



// implementation for MegachatRequestListener
void MegaChatApplication::onRequestFinish(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRequest *request, megachat::MegaChatError* e)
{
    switch (request->getType())
    {
        case megachat::MegaChatRequest::TYPE_CONNECT:
            if (e->getErrorCode() == mega::MegaError::API_OK)
            {
               addChats();

               int a=0;
            }
            else
            {
            }
            break;
    }
}





void MegaChatApplication::onRequestStart(megachat::MegaChatApi* api, megachat::MegaChatRequest *request){}
void MegaChatApplication::onRequestUpdate(megachat::MegaChatApi*api, megachat::MegaChatRequest *request){}
void MegaChatApplication::onRequestTemporaryError(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError* error){}

// implementation for MegachatCallListener
void MegaChatApplication::onChatCallUpdate(megachat::MegaChatApi* api, megachat::MegaChatCall *call){}

std::string MegaChatApplication::getAppDir() const
{
    return appDir;
}
//------------------------------------------------------------------------------------------------------>


/*
IMPLEMENT THIS FUNCTION IN THIS CLASS
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
*/
//------------------------------------------------------------------------------------------------------>


MegaLoggerApplication::MegaLoggerApplication(const char *filename)
{
    testlog.open(filename, ios::out | ios::app);
}

MegaLoggerApplication::~MegaLoggerApplication()
{
    testlog.close();
}

void MegaLoggerApplication::log(const char *time, int loglevel, const char *source, const char *message)
{
    testlog << "[" << time << "] " << SimpleLogger::toStr((LogLevel)loglevel) << ": ";
    testlog << message << " (" << source << ")" << endl;
}

void MegaLoggerApplication::postLog(const char *message)
{
    testlog << message << endl;
}

void MegaLoggerApplication::log(int loglevel, const char *message)
{
    string levelStr;
    switch (loglevel)
    {
        case MegaChatApi::LOG_LEVEL_ERROR: levelStr = "err"; break;
        case MegaChatApi::LOG_LEVEL_WARNING: levelStr = "warn"; break;
        case MegaChatApi::LOG_LEVEL_INFO: levelStr = "info"; break;
        case MegaChatApi::LOG_LEVEL_VERBOSE: levelStr = "verb"; break;
        case MegaChatApi::LOG_LEVEL_DEBUG: levelStr = "debug"; break;
        case MegaChatApi::LOG_LEVEL_MAX: levelStr = "debug-verbose"; break;
        default: levelStr = ""; break;
    }
    testlog  << message;
}
