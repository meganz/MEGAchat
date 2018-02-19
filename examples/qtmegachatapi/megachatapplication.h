#ifndef MEGACHATAPPLICATION_H
#define MEGACHATAPPLICATION_H
#ifdef _WIN32
    #include <winsock2.h>
#endif
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
using namespace strophe;

namespace megachatapp {
    class MegaChatApplication;
    class AppDelegatea;
    struct GcmEvent;
}

struct GcmEvent: public QEvent
{
    static const QEvent::Type type;
    void* ptr;
    GcmEvent(void* aPtr): QEvent(type), ptr(aPtr){}
};


class MegaLoggerApplication : public mega::MegaLogger,
        public megachat::MegaChatLogger
{
        public:
            MegaLoggerApplication(const char *filename);
            ~MegaLoggerApplication();

            std::ofstream *getOutputStream() { return &testlog; }
            void postLog(const char *message);

        private:
            std::ofstream testlog;

        protected:
            void log(const char *time, int loglevel, const char *source, const char *message);
            void log(int loglevel, const char *message);
};


class MegaChatApplication:
        public QApplication,
        public mega::MegaListener,
        public megachat::MegaChatListener,
        public megachat::MegaChatRequestListener
{
    Q_OBJECT
    public:
         MegaChatApplication(int &argc ,char** argv);
         void init();
         void login();
         void logout();
         void readSid();
         const char *getSid() const;
         void sigintHandler(int);
         void saveSid(const char* sdkSid);
         void configureLogs();
         void addChats();
         std::string getAppDir() const;
         ~MegaChatApplication();

         //----------------------------------------------------------------------------------------------------------------------------------->
         // implementation for Megachatlistener
         virtual void onChatInitStateUpdate(megachat::MegaChatApi *api, int newState);
         virtual void onChatListItemUpdate(megachat::MegaChatApi* api, megachat::MegaChatListItem *item);
         virtual void onChatOnlineStatusUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle userhandle, int status, bool inProgress);
         virtual void onChatPresenceConfigUpdate(megachat::MegaChatApi* api, megachat::MegaChatPresenceConfig *config);
         virtual void onChatConnectionStateUpdate(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, int state);

         // implementation for MegachatRequestListener
         virtual void onRequestStart(megachat::MegaChatApi* api, megachat::MegaChatRequest *request);

         //virtual void onRequestFinish(megachat::MegaChatApi* api, megachat::MegaChatRequest *request, megachat::MegaChatError* e);
         virtual void onRequestFinish(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRequest *request, megachat::MegaChatError* e);
         virtual void onRequestUpdate(megachat::MegaChatApi*api, megachat::MegaChatRequest *request);
         virtual void onRequestTemporaryError(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError* error);

         // implementation for MegaRequestListener
         virtual void onRequestStart(mega::MegaApi *api, mega::MegaRequest *request) {}
         virtual void onRequestUpdate(mega::MegaApi*api, mega::MegaRequest *request) {}
         virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, MegaError *e);
         virtual void onRequestTemporaryError(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError* error) {}

         // implementation for MegachatCallListener
         virtual void onChatCallUpdate(megachat::MegaChatApi* api, megachat::MegaChatCall *call);
         //----------------------------------------------------------------------------------------------------------------------------------->

    private:
         char* sid = nullptr;
         MainWindow* mainWin;
         mega::MegaApi *megaApi;
         std::string appDir;
         megachat::MegaChatApi *megaChatApi;
         LoginDialog *loginDlg;
         MegaLoggerApplication *logger;


    public slots:
        void onAppTerminate();
};
#endif // MEGACHATAPPLICATION_H


