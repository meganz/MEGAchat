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

class MegaLoggerApplication;

class MegaChatApplication : public QApplication,
        public mega::MegaListener,
        public megachat::MegaChatRequestListener
{
    Q_OBJECT

public:
    MegaChatApplication(int &argc, char** argv);
    virtual ~MegaChatApplication();

    void init();
    void login();
    void logout();
    void readSid();

    void saveSid(const char* sdkSid);
    void configureLogs();
    void addChats();
    void addContacts();
    std::string getAppDir() const;

    virtual void onRequestFinish(megachat::MegaChatApi* megaChatApi, megachat::MegaChatRequest *request, megachat::MegaChatError* e);
    virtual void onRequestFinish(mega::MegaApi *api, mega::MegaRequest *request, mega::MegaError *e);
    virtual void onUsersUpdate(mega::MegaApi * api, mega::MegaUserList * userList);

protected:
    char* sid;
    std::string appDir;
    LoginDialog *loginDialog;
    MainWindow *mainWin;
    MegaLoggerApplication *logger;
    mega::QTMegaListener *megaListenerDelegate;
    megachat::QTMegaChatRequestListener *megaChatRequestListenerDelegate;
    mega::MegaApi *megaApi;
    megachat::MegaChatApi *megaChatApi;

public slots:
    void onLoginClicked();
};

#endif // MEGACHATAPPLICATION_H
