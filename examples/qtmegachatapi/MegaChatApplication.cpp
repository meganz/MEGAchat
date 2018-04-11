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

int main(int argc, char **argv)
{
    struct sigaction sa;
    MegaChatApplication app(argc,argv);
    app.readSid();
    app.init();
    return app.exec();
}

MegaChatApplication::MegaChatApplication(int &argc, char **argv) : QApplication(argc, argv)
{
    mAppDir = MegaChatApi::getAppDir();
    configureLogs();

    // Keep the app open until it's explicitly closed
    setQuitOnLastWindowClosed(true);

    mLoginDialog = NULL;
    mSid = NULL;

    // Initialize the SDK and MEGAchat
    mMegaApi = new MegaApi("karere-native", mAppDir.c_str(), "Karere Native");
    megaChatApi = new MegaChatApi(mMegaApi);

    // Create delegate listeners
    megaListenerDelegate = new QTMegaListener(mMegaApi, this);
    mMegaApi->addListener(megaListenerDelegate);

    megaChatRequestListenerDelegate = new QTMegaChatRequestListener(megaChatApi, this);
    megaChatApi->addChatRequestListener(megaChatRequestListenerDelegate);

    megaChatNotificationListenerDelegate = new QTMegaChatNotificationListener(megaChatApi, this);
    megaChatApi->addChatNotificationListener(megaChatNotificationListenerDelegate);

    // Start GUI
    mMainWin = new MainWindow(0, mLogger);
    mMainWin->setMegaChatApi(megaChatApi);
    mMainWin->setMegaApi(mMegaApi);
}

MegaChatApplication::~MegaChatApplication()
{
    delete megaListenerDelegate;
    delete megaChatRequestListenerDelegate;
    delete megaChatApi;
    delete mMegaApi;
    delete mMainWin;
    delete mLogger;
    delete [] mSid;
}

void MegaChatApplication::init()
{
    int initState = megaChatApi->init(mSid);
    if (!mSid)
    {
        assert(initState == MegaChatApi::INIT_WAITING_NEW_SESSION);
        login();
    }
    else
    {
        assert(initState == MegaChatApi::INIT_OFFLINE_SESSION
               || initState == MegaChatApi::INIT_NO_CACHE);
        mMegaApi->fastLogin(mSid);
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
    mMegaApi->login(email.toUtf8().constData(), password.toUtf8().constData());
}

void MegaChatApplication::logout()
{
    mMegaApi->logout();
}

void MegaChatApplication::readSid()
{
    char buf[256];
    ifstream sidf(mAppDir + "/sid");
    if (!sidf.fail())
    {
       sidf.getline(buf, 256);
       if (!sidf.fail())
           mSid = strdup(buf);
    }
}

void MegaChatApplication::saveSid(const char* sdkSid)
{
    ofstream osidf(mAppDir + "/sid");
    assert(sdkSid);
    osidf << sdkSid;
    osidf.close();
}

void MegaChatApplication::configureLogs()
{
    std::string logPath = mAppDir + "/log.txt";
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
    MegaUserList *contactList = mMegaApi->getContacts();

    for (int i=0; i<contactList->size(); i++)
    {
        contact = contactList->get(i);
        const char *contactEmail = contact->getEmail();
        megachat::MegaChatHandle userHandle = megaChatApi->getUserHandleByEmail(contactEmail);
        if (megachat::MEGACHAT_INVALID_HANDLE != userHandle)
            mMainWin->addContact(contact);
    }
    delete contactList;
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
            std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
            itContacts = this->mMainWin->contactWidgets.find(userHandle);
            if (itContacts == this->mMainWin->contactWidgets.end())
            {
                mMainWin->addContact(user);
            }
            else
            {
                if (userList->get(i)->hasChanged(MegaUser::CHANGE_TYPE_FIRSTNAME))
                {
                    megaChatApi->getUserFirstname(userHandle);
                }
                else if (user->getVisibility() == MegaUser::VISIBILITY_HIDDEN && mMainWin->allItemsVisibility != true)
                {
                    mMainWin->orderContactChatList(mMainWin->allItemsVisibility);
                }
            }
        }
    }
}

void MegaChatApplication::onChatNotification(MegaChatApi *, MegaChatHandle chatid, MegaChatMessage *msg)
{
    const char *chat = mMegaApi->userHandleToBase64((MegaHandle)chatid);
    const char *msgid = mMegaApi->userHandleToBase64((MegaHandle)msg->getMsgId());

    string log("Chat notification received in chat [");
    log.append(chat);
    log.append("], msgid: ");
    log.append(msgid);
    mLogger->postLog(log.c_str());

    delete chat;
    delete msgid;
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
                delete [] mSid;
                mSid = mMegaApi->dumpSession();
                saveSid(mSid);
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
            if (e->getErrorCode() == MegaChatError::ERROR_OK)
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
             if (e->getErrorCode() == MegaChatError::ERROR_OK)
             {
                MegaChatHandle userHandle = request->getUserHandle();
                const char *firstname = request->getText();
                mMainWin->updateContactFirstname(userHandle,firstname);
             }
             break;
         case MegaChatRequest::TYPE_CREATE_CHATROOM:
             if (e->getErrorCode() == MegaChatError::ERROR_OK)
             {
                std::string title;
                MegaChatHandle handle = request->getChatHandle();
                QString qTitle = QInputDialog::getText(this->mMainWin, tr("Change chat title"), tr("Leave blank for default title"));
                if (!qTitle.isNull())
                {
                    title = qTitle.toStdString();
                    if (!title.empty())
                    {
                        this->megaChatApi->setChatTitle(handle, title.c_str());
                    }
                }

                const MegaChatListItem* chatListItem = this->megaChatApi->getChatListItem(handle);
                mMainWin->addChat(chatListItem);
                delete chatListItem;
             }
             break;
         case MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
                QMessageBox::critical(nullptr, tr("Leave chat"), tr("Error leaving chat: ").append(e->getErrorString()));
            break;
         case MegaChatRequest::TYPE_EDIT_CHATROOM_NAME:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
                QMessageBox::critical(nullptr, tr("Edit chat topic"), tr("Error modifiying chat topic: ").append(e->getErrorString()));
            break;
    }
}

