#include "MegaChatApplication.h"
#include "megaLoggerApplication.h"
#include <iostream>
#include <QDir>
#include <QInputDialog>
#include <QMessageBox>
#include <assert.h>
#include <sys/stat.h>
#include "signal.h"
#include <QClipboard>

using namespace std;
using namespace mega;
using namespace megachat;

int main(int argc, char **argv)
{
    MegaChatApplication app(argc, argv);
    app.init();
    return app.exec();
}

MegaChatApplication::MegaChatApplication(int &argc, char **argv) : QApplication(argc, argv)
{
    mAppDir = MegaChatApi::getAppDir();
    configureLogs();

    // Keep the app open until it's explicitly closed
    setQuitOnLastWindowClosed(true);

    mMainWin = NULL;
    mLoginDialog = NULL;
    mSid = NULL;

    // Initialize the SDK and MEGAchat
    mMegaApi = new MegaApi("karere-native", mAppDir.c_str(), "Karere Native");
    mMegaChatApi = new MegaChatApi(mMegaApi);

    // Create delegate listeners
    megaListenerDelegate = new QTMegaListener(mMegaApi, this);
    mMegaApi->addListener(megaListenerDelegate);

    megaChatRequestListenerDelegate = new QTMegaChatRequestListener(mMegaChatApi, this);
    mMegaChatApi->addChatRequestListener(megaChatRequestListenerDelegate);

    megaChatNotificationListenerDelegate = new QTMegaChatNotificationListener(mMegaChatApi, this);
    mMegaChatApi->addChatNotificationListener(megaChatNotificationListenerDelegate);
}

MegaChatApplication::~MegaChatApplication()
{
    mMegaApi->httpServerStop();
    mMegaApi->removeListener(megaListenerDelegate);
    mMegaChatApi->removeChatRequestListener(megaChatRequestListenerDelegate);
    mMegaChatApi->removeChatNotificationListener(megaChatNotificationListenerDelegate);
    delete megaChatNotificationListenerDelegate;
    delete megaChatRequestListenerDelegate;
    delete megaListenerDelegate;
    delete mMainWin;
    delete mMegaChatApi;
    delete mMegaApi;
    delete mLogger;
    delete [] mSid;
}

void MegaChatApplication::init()
{
    mFirstnamesMap.clear();
    mFirstnameFetching.clear();
    if (mMainWin)
    {
        mMainWin->deleteLater();
        mMainWin = NULL;
    }
    if (mLoginDialog)
    {
        mLoginDialog->deleteLater();
        mLoginDialog = NULL;
    }
    delete [] mSid;
    mSid = NULL;

    mMainWin = new MainWindow((QWidget *)this, mLogger, mMegaChatApi, mMegaApi);

    mSid = readSid();
    int initState = mMegaChatApi->init(mSid);
    if (!mSid)
    {
        assert(initState == MegaChatApi::INIT_WAITING_NEW_SESSION);
        login();
    }
    else
    {
        assert(initState == MegaChatApi::INIT_OFFLINE_SESSION
               || initState == MegaChatApi::INIT_NO_CACHE);

        mMegaChatApi->enableGroupChatCalls(true);
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
    if (mMegaChatApi->getInitState() == MegaChatApi::INIT_NOT_DONE)
    {
        int initState = mMegaChatApi->init(mSid);
        assert(initState == MegaChatApi::INIT_WAITING_NEW_SESSION);
        mMegaChatApi->enableGroupChatCalls(true);
    }
    mMegaApi->login(email.toUtf8().constData(), password.toUtf8().constData());
}

void MegaChatApplication::logout()
{
    mMegaApi->logout();
}

const char *MegaChatApplication::readSid()
{
    char buf[256];
    ifstream sidf(mAppDir + "/sid");
    if (!sidf.fail())
    {
       sidf.getline(buf, 256);
       if (!sidf.fail())
       {
           return MegaApi::strdup(buf);
       }
    }
    return NULL;
}

void MegaChatApplication::saveSid(const char *sid)
{
    assert(sid);
    ofstream osidf(mAppDir + "/sid");
    osidf << sid;
    osidf.close();
}

void MegaChatApplication::removeSid()
{
    std::string sidPath = mAppDir + "/sid";
    std::remove(sidPath.c_str());
}

void MegaChatApplication::configureLogs()
{
    std::string logPath = mAppDir + "/log.txt";
    mLogger = new MegaLoggerApplication(logPath.c_str());
    MegaApi::setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
    MegaChatApi::setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
    MegaChatApi::setLoggerObject(mLogger);
    MegaChatApi::setLogToConsole(true);
    MegaChatApi::setCatchException(false);
}

void MegaChatApplication::onUsersUpdate(::mega::MegaApi *, ::mega::MegaUserList *userList)
{
    if(mMainWin && userList)
    {
        mMainWin->addOrUpdateContactControllersItems(userList);
        mMainWin->reorderAppContactList();
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

    delete [] chat;
    delete [] msgid;
}

const char *MegaChatApplication::getFirstname(MegaChatHandle uh)
{
    if (uh == mMegaChatApi->getMyUserHandle())
    {
        return MegaApi::strdup("Me");
    }

    if (uh == 13041471603018644609U)   // handle for API user / Commander
    {
        return MegaApi::strdup("API-user");
    }

    auto it = mFirstnamesMap.find(uh);
    if (it != mFirstnamesMap.end())
    {
        return MegaApi::strdup(it->second.c_str());
    }

    if (!mFirstnameFetching[uh])
    {
        mMegaChatApi->getUserFirstname(uh);
        mFirstnameFetching[uh] = true;
    }

    return NULL;
}

const char *MegaChatApplication::sid() const
{
    return mSid;
}

void MegaChatApplication::resetLoginDialog()
{
    mLoginDialog->deleteLater();
    mLoginDialog = NULL;
}

LoginDialog *MegaChatApplication::loginDialog() const
{
    return mLoginDialog;
}

void MegaChatApplication::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    switch (request->getType())
    {
        case MegaRequest::TYPE_LOGIN:
            if (e->getErrorCode() == MegaError::API_EMFAREQUIRED)
            {
                std::string auxcode = this->mMainWin->getAuthCode();
                if (!auxcode.empty())
                {
                    QString email(request->getEmail());
                    QString password(request->getPassword());
                    QString code(auxcode.c_str());
                    mMegaApi->multiFactorAuthLogin(email.toUtf8().constData(), password.toUtf8().constData(), code.toUtf8().constData());
                }
                else
                {
                    login();
                }
            }
            else if (e->getErrorCode() == MegaError::API_OK)
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
                if (!mSid)
                {
                    mSid = mMegaApi->dumpSession();
                    saveSid(mSid);
                }
                mMegaChatApi->connect();
            }
            else
            {
                QMessageBox::critical(nullptr, tr("Fetch Nodes"), tr("Error Fetching nodes: ").append(e->getErrorString()));
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

        case MegaRequest::TYPE_MULTI_FACTOR_AUTH_CHECK:
            if (e->getErrorCode() == MegaError::API_OK)
            {
                this->mMainWin->createFactorMenu(request->getFlag());
            }
            break;

        case MegaRequest::TYPE_MULTI_FACTOR_AUTH_GET:
            if (e->getErrorCode() == MegaError::API_OK)
            {
                QClipboard *clipboard = QApplication::clipboard();
                QMessageBox msg;
                msg.setIcon(QMessageBox::Information);
                msg.setWindowTitle("2FA Code:");
                msg.setText(request->getText());

                QAbstractButton *copyButton = msg.addButton(tr("Copy to clipboard"), QMessageBox::ActionRole);
                copyButton->disconnect();
                connect(copyButton, &QAbstractButton::clicked, this, [=](){clipboard->setText(request->getText());});
                QAbstractButton *contButton = msg.addButton(tr("Continue"), QMessageBox::ActionRole);
                msg.exec();

                std::string auxcode = this->mMainWin->getAuthCode();
                if (!auxcode.empty())
                {
                    QString code(auxcode.c_str());
                    mMegaApi->multiFactorAuthEnable(code.toUtf8().constData());
                }
            }
            break;

        case MegaRequest::TYPE_MULTI_FACTOR_AUTH_SET:
            QString text;
            if (request->getFlag())
            {
                text.append("Enable 2FA");
            }
            else
            {
                text.append("Disable 2FA");
            }

            if (e->getErrorCode() == MegaError::API_OK)
            {
                QMessageBox::warning(nullptr, tr(text.toStdString().c_str()), tr("The operation has been completed successfully"));
            }
            else
            {
                QMessageBox::warning(nullptr, tr(text.toStdString().c_str()), tr(" ").append(e->getErrorString()));
            }
            break;
    }
}

void MegaChatApplication::onRequestFinish(MegaChatApi *, MegaChatRequest *request, MegaChatError *e)
{
    switch (request->getType())
    {
         case MegaChatRequest::TYPE_CONNECT:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
            {
                QMessageBox::critical(nullptr, tr("Chat Connection"), tr("Error stablishing connection").append(e->getErrorString()));
                init();
            }
            else
            {
                MegaUserList *contactList = mMegaApi->getContacts();
                mMainWin->addOrUpdateContactControllersItems(contactList);
                mMainWin->reorderAppContactList();
                delete contactList;
            }
            break;
          case MegaChatRequest::TYPE_LOGOUT:
            if (e->getErrorCode() == MegaChatError::ERROR_OK)
            {
                removeSid();
                init();
            }
            break;
         case MegaChatRequest::TYPE_GET_FIRSTNAME:
             {
             MegaChatHandle userHandle = request->getUserHandle();
             int errorCode = e->getErrorCode();
             if (errorCode == MegaChatError::ERROR_OK)
             {
                const char *firstname = request->getText();
                if ((strlen(firstname)) == 0)
                {
                    this->mMegaChatApi->getUserEmail(userHandle);
                    break;
                }
                mFirstnamesMap[userHandle] = firstname;
                mFirstnameFetching[userHandle] = false;
                mMainWin->updateContactFirstname(userHandle,firstname);
                mMainWin->updateMessageFirstname(userHandle,firstname);
             }
             else if (errorCode == MegaChatError::ERROR_NOENT)
             {
                this->mMegaChatApi->getUserEmail(userHandle);
             }
             break;
             }
         case MegaChatRequest::TYPE_GET_EMAIL:
            {
            MegaChatHandle userHandle = request->getUserHandle();
            if (e->getErrorCode() == MegaChatError::ERROR_OK)
            {
               const char *email = request->getText();
               mMainWin->updateContactFirstname(userHandle,email);
               mMainWin->updateMessageFirstname(userHandle,email);
            }
            else
            {
               mMainWin->updateMessageFirstname(userHandle,"Unknown contact");
            }
            break;
            }
         case MegaChatRequest::TYPE_CREATE_CHATROOM:
             if (e->getErrorCode() == MegaChatError::ERROR_OK)
             {
                std::string title;
                MegaChatHandle chatid = request->getChatHandle();
                const MegaChatListItem *chatListItem = mMegaChatApi->getChatListItem(chatid);

                //Set chat title
                if (chatListItem->isGroup())
                {
                    QString qTitle = QInputDialog::getText(this->mMainWin, tr("Change chat title"), tr("Leave blank for default title"));
                    if (!qTitle.isNull())
                    {
                        title = qTitle.toStdString();
                        if (!title.empty())
                        {
                            this->mMegaChatApi->setChatTitle(chatid, title.c_str());
                        }
                    }
                }
                else
                {
                    if (mMainWin->getChatControllerById(chatid))
                    {
                        QMessageBox::warning(nullptr, tr("Chat creation"), tr("1on1 chat already existed"));
                        delete chatListItem;
                        break;
                    }
                }
                mMainWin->addOrUpdateChatControllerItem(chatListItem->copy());
                mMainWin->reorderAppChatList();
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

        case MegaChatRequest::TYPE_ARCHIVE_CHATROOM:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
            {
                QMessageBox::critical(nullptr, tr("Archive chat"), tr("Error archiving chat: ").append(e->getErrorString()));
            }
            break;

#ifndef KARERE_DISABLE_WEBRTC
         case MegaChatRequest::TYPE_ANSWER_CHAT_CALL:
         case MegaChatRequest::TYPE_START_CHAT_CALL:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
              {
                QMessageBox::critical(nullptr, tr("Call"), tr("Error in call: ").append(e->getErrorString()));
              }
            else
            {
                megachat::MegaChatHandle chatId = request->getChatHandle();
                ChatListItemController *itemController = mMainWin->getChatControllerById(chatId);
                if(itemController)
                {
                    ChatWindow *chatWin = itemController->showChatWindow();
                    chatWin->connectPeerCallGui(mMegaChatApi->getMyUserHandle());
                }
            }
            break;

          case MegaChatRequest::TYPE_HANG_CHAT_CALL:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
              {
                QMessageBox::critical(nullptr, tr("Call"), tr("Error in call: ").append(e->getErrorString()));
              }
            else
            {
                megachat::MegaChatHandle chatId = request->getChatHandle();

                ChatListItemController *itemController = mMainWin->getChatControllerById(chatId);

                if (itemController)
                {
                    ChatItemWidget *widget = itemController->getWidget();
                    if (widget)
                    {
                        ChatWindow *chatWin= itemController->showChatWindow();
                        if(chatWin)
                        {
                            chatWin->hangCall();
                        }
                    }
                }
            }
            break;

         case MegaChatRequest::TYPE_LOAD_AUDIO_VIDEO_DEVICES:
                mMainWin->createSettingsMenu();
            break;
#endif
        case MegaChatRequest::TYPE_ATTACH_NODE_MESSAGE:
            if (e->getErrorCode() != MegaChatError::ERROR_OK)
            {
                QMessageBox::critical(nullptr, tr("Attachment"), tr("Error in attachment: ").append(e->getErrorString()));
            }
            else
            {
                MegaChatHandle chatid = request->getChatHandle();
                MegaChatMessage *msg = request->getMegaChatMessage();
                ChatWindow *window = mMainWin->getChatWindowIfExists(chatid);
                if (window)
                {
                    window->onMessageReceived(mMegaChatApi, msg);
                }
            }
            break;
    }
}
