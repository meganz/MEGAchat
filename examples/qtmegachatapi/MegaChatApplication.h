#ifndef MEGACHATAPPLICATION_H
#define MEGACHATAPPLICATION_H
#include <fstream>
#include <QApplication>
#include "LoginDialog.h"
#include "MainWindow.h"
#include "megachatapi.h"
#include "QTMegaListener.h"
#include "QTMegaChatListener.h"
#include "QTMegaChatRequestListener.h"
#include "QTMegaChatRoomListener.h"
#include "QTMegaChatNotificationListener.h"

#define MAX_RETRIES 5
class MegaLoggerApplication;

class MegaChatApplication : public QApplication,
    public mega::MegaListener,
    public megachat::MegaChatRequestListener,
    public megachat::MegaChatNotificationListener
{
    Q_OBJECT
    public:
        MegaChatApplication(int &argc, char** argv);
        virtual ~MegaChatApplication();
        void init();
        void login();
        void logout();
        void readSid();
        void addChats();
        void addContacts();
        void configureLogs();
        void saveSid(const char* sdkSid);
        const char *loginCode();
        virtual void onRequestFinish(megachat::MegaChatApi *mMegaChatApi, megachat::MegaChatRequest *request, megachat::MegaChatError *e);
        virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError *e);
        virtual void onUsersUpdate(mega::MegaApi * api, mega::MegaUserList * userList);
        virtual void onChatNotification(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, megachat::MegaChatMessage *msg);

    protected:
        char* mSid;
        std::string mAppDir;
        MainWindow *mMainWin;
        LoginDialog *mLoginDialog;
        MegaLoggerApplication *mLogger;
        mega::MegaApi *mMegaApi;
        megachat::MegaChatApi *mMegaChatApi;
        mega::QTMegaListener *megaListenerDelegate;
        megachat::QTMegaChatRequestListener *megaChatRequestListenerDelegate;
        megachat::QTMegaChatNotificationListener *megaChatNotificationListenerDelegate;

    public slots:
        void onLoginClicked();
};
#endif // MEGACHATAPPLICATION_H
