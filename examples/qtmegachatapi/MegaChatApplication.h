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
#define MAX_RETRIES 5
class MegaLoggerApplication;

class MegaChatApplication : public QApplication,
    public mega::MegaListener,
    public megachat::MegaChatRequestListener
{
    Q_OBJECT
    public:
        MegaChatApplication(int &argc, char** argv);
        virtual ~MegaChatApplication();
        std::string getAppDir() const;
        void init();
        void login();
        void logout();
        void readSid();
        void addChats();
        void addContacts();
        void configureLogs();
        void saveSid(const char* sdkSid);
        virtual void onRequestFinish(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRequest *request, megachat::MegaChatError* e);
        virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError *e);
        virtual void onUsersUpdate(mega::MegaApi * api, mega::MegaUserList * userList);

    protected:
        char* sid;
        int connectionRetries;
        std::string appDir;
        MainWindow *mainWin;
        LoginDialog *loginDialog;
        MegaLoggerApplication *logger;
        mega::MegaApi *megaApi;
        megachat::MegaChatApi *megaChatApi;
        mega::QTMegaListener *megaListenerDelegate;
        megachat::QTMegaChatRequestListener *megaChatRequestListenerDelegate;

    public slots:
        void onLoginClicked();
};
#endif // MEGACHATAPPLICATION_H
