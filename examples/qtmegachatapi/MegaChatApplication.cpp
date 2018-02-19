#include "MegaChatApplication.h"
#include "megaLoggerApplication.h"
#include <iostream>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <assert.h>
#include <sys/stat.h>
#include "signal.h"

using namespace std;
using namespace mega;
using namespace megachat;

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    exit(0);
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    MegaChatApplication app(argc,argv);
    app.readSid();
    app.init();
    sigaction(SIGSEGV, &sa, NULL);
    return app.exec();
}

MegaChatApplication::MegaChatApplication(int &argc, char **argv) : QApplication(argc, argv)
{
    appDir = QDir::homePath().toStdString() + "/.karere";
    struct stat st = {0};
    if (stat(appDir.c_str(), &st) == -1)
    {
        mkdir(appDir.c_str(), 0700);
    }

    configureLogs();

    // Keep the app open until it's explicitly closed
    setQuitOnLastWindowClosed(true);

    mLoginDialog = NULL;
    sid = NULL;

    // Initialize the SDK and MEGAchat
    megaApi = new MegaApi("karere-native", appDir.c_str(), "Karere Native");
    megaChatApi = new MegaChatApi(megaApi);

    // Create delegate listeners
    megaListenerDelegate = new QTMegaListener(megaApi, this);
    megaChatRequestListenerDelegate = new QTMegaChatRequestListener(megaChatApi, this);
    megaApi->addListener(megaListenerDelegate);
    megaChatApi->addChatRequestListener(megaChatRequestListenerDelegate);


    // Start GUI
    mMainWin = new MainWindow(0, mLogger);
    mMainWin->setMegaChatApi(megaChatApi);
    mMainWin->setMegaApi(megaApi);
}

MegaChatApplication::~MegaChatApplication()
{
    delete megaListenerDelegate;
    delete megaChatRequestListenerDelegate;
    delete megaChatApi;
    delete megaApi;
    delete mMainWin;
    delete mLogger;
    delete [] sid;
}

void MegaChatApplication::init()
{
    int initState = megaChatApi->init(sid);
    if (!sid)
    {
        assert(initState == MegaChatApi::INIT_WAITING_NEW_SESSION);
        login();
    }
    else
    {
        assert(initState == MegaChatApi::INIT_OFFLINE_SESSION);
        megaApi->fastLogin(sid);
    }
}

void MegaChatApplication::login()
{
    mLoginDialog = new LoginDialog();
    connect(mLoginDialog, SIGNAL(onLoginClicked()), this, SLOT(onLoginClicked()));
    mLoginDialog->show();
}

void MegaChatApplication::onLoginClicked()
{
    QString email = mLoginDialog->getEmail();
    QString password = mLoginDialog->getPassword();
    mLoginDialog->setState(LoginDialog::loggingIn);
    megaApi->login(email.toUtf8().constData(), password.toUtf8().constData());
}

void MegaChatApplication::logout()
{
    megaApi->logout();
}

void MegaChatApplication::readSid()
{
    char buf[256];
    ifstream sidf(appDir + "/sid");
    if (!sidf.fail())
    {
       sidf.getline(buf, 256);
       if (!sidf.fail())
           sid = strdup(buf);
    }
}

void MegaChatApplication::saveSid(const char* sdkSid)
{
    ofstream osidf(appDir + "/sid");
    assert(sdkSid);
    osidf << sdkSid;
    osidf.close();
}

void MegaChatApplication::configureLogs()
{
    std::string logPath = appDir + "/log.txt";
    mLogger = new MegaLoggerApplication(logPath.c_str());
    mLogger->setLogConsole(true);
    MegaApi::setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
    MegaChatApi::setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    MegaChatApi::setLoggerObject(mLogger);
    MegaChatApi::setLogToConsole(true);
    MegaChatApi::setCatchException(false);
}

void MegaChatApplication::addChats()
{
    MegaChatListItemList * chatList = megaChatApi->getChatListItems();
    for (int i = 0; i < chatList->size(); i++)
    {
        mMainWin->addChat(chatList->get(i));
    }
    mMainWin->addChatListener();
    delete chatList;
}


void MegaChatApplication::addContacts()
{
    MegaUser * contact = NULL;
    MegaUserList *contactList = megaApi->getContacts();
    mMainWin->setNContacts(contactList->size());

    for (int i=0; i<contactList->size(); i++)
    {
        contact = contactList->get(i);
        const char *contactEmail = contact->getEmail();
        megachat::MegaChatHandle userHandle = megaChatApi->getUserHandleByEmail(contactEmail);
        if (megachat::MEGACHAT_INVALID_HANDLE != userHandle)
            mMainWin->addContact(userHandle);
    }
    delete contactList;
}


std::string MegaChatApplication::getAppDir() const
{
    return appDir;
}


void MegaChatApplication::onUsersUpdate(mega::MegaApi * api, mega::MegaUserList * userList)
{
    mega::MegaHandle userHandle = NULL;
    mega:MegaUser *user;

    if(userList)
    {
        for(int i=0; i<userList->size(); i++)
        {
            user = userList->get(i);
            userHandle = userList->get(i)->getHandle();
            if(userList->get(i)->hasChanged(MegaUser::CHANGE_TYPE_FIRSTNAME))
                megaChatApi->getUserFirstname(userHandle);
        }
    }
}


void MegaChatApplication::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    switch (request->getType())
    {
        case MegaRequest::TYPE_LOGIN:
            if (e->getErrorCode() == MegaError::API_OK)
            {
                if (mLoginDialog)
                {
                    mLoginDialog->setState(LoginDialog::fetchingNodes);
                }
                api->fetchNodes();
            }
            else
            {
                if (mLoginDialog)
                {
                    mLoginDialog->setState(LoginDialog::badCredentials);
                    mLoginDialog->enableControls(true);
                }
                else
                {
                    login();
                }
            }
            break;
        case MegaRequest::TYPE_FETCH_NODES:
            if (e->getErrorCode() == MegaError::API_OK)
            {
                delete [] sid;
                sid = megaApi->dumpSession();
                saveSid(sid);
                mLoginDialog->deleteLater();
                mLoginDialog = NULL;
                mMainWin->setWindowTitle(api->getMyEmail());
                mMainWin->show();
                addContacts();
                megaChatApi->connect();
            }
            else
            {
                QMessageBox::critical(nullptr, tr("Fetch Nodes"), tr("Error Fetching nodes: ").append(e->getErrorString()));
                mLoginDialog->deleteLater();
                mLoginDialog = NULL;
                init();
            }
            break;
        case MegaRequest::TYPE_REMOVE_CONTACT:
            if (e->getErrorCode() != MegaError::API_OK)
                QMessageBox::critical(nullptr, tr("Remove contact"), tr("Error removing contact: ").append(e->getErrorString()));
            break;

        case MegaRequest::TYPE_INVITE_CONTACT:
            if (e->getErrorCode() != MegaError::API_OK)
                QMessageBox::critical(nullptr, tr("Invite contact"), tr("Error inviting contact: ").append(e->getErrorString()));
            break;
    }
}

void MegaChatApplication::onRequestFinish(MegaChatApi* megaChatApi, MegaChatRequest *request, MegaChatError* e)
{
    switch (request->getType())
    {
         case MegaChatRequest::TYPE_CONNECT:
            if (e->getErrorCode() == MegaError::API_OK)
            {
                addChats();
            }
            else
            {
                QMessageBox::critical(nullptr, tr("Chat Connection"), tr("Error stablishing connection").append(e->getErrorString()));
                mLoginDialog->deleteLater();
                mLoginDialog = NULL;
                init();
            }
            break;
         case MegaChatRequest::TYPE_GET_FIRSTNAME:
             if (e->getErrorCode() == MegaError::API_OK)
             {
                MegaChatHandle userHandle = request->getUserHandle();
                const char *firstname = request->getText();
                mMainWin->updateContactFirstname(userHandle,firstname);
             }
             break;
         case MegaChatRequest::TYPE_CREATE_CHATROOM:
             if (e->getErrorCode() == MegaError::API_OK)
             {
                std::string title;
                MegaChatHandle handle = request->getChatHandle();
                QString qTitle = QInputDialog::getText(this->mMainWin, tr("Change chat title"), tr("Leave blank for default title"));
                if (! qTitle.isNull())
                    title = qTitle.toStdString();

                this->megaChatApi->setChatTitle(handle, title.c_str());
                const MegaChatListItem* chatListItem = this->megaChatApi->getChatListItem(handle);
                mMainWin->addChat(chatListItem);
                delete chatListItem;
             }
             break;
         case MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM:
            switch (e->getErrorCode())
                case MegaChatError::ERROR_ACCESS:
                    QMessageBox::critical(nullptr, tr("Leave chat"), tr("Error leaving chat: ").append(e->getErrorString()));
                    break;
            break;
         case MegaChatRequest::TYPE_EDIT_CHATROOM_NAME:
            switch (e->getErrorCode())
                case MegaChatError::ERROR_ACCESS:
                        QMessageBox::critical(nullptr, tr("Edit chat topic"), tr("Error modifiying chat topic: ").append(e->getErrorString()));
                        break;
                break;
            break;
    }
}

