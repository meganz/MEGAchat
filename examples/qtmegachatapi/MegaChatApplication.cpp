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
    mMegaApi = new MegaApi("karere-native", mAppDir.c_str(), "MEGAChatQtApp");
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
    mNotificationSettings.reset();
    mTimeZoneDetails.reset();
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
    if (!mSid)
    {
        login();
    }
    else
    {
        int initState = mMegaChatApi->init(mSid);
        assert(initState == MegaChatApi::INIT_OFFLINE_SESSION
               || initState == MegaChatApi::INIT_NO_CACHE);

        mMegaApi->fastLogin(mSid);
    }
}

void MegaChatApplication::login()
{
   mLoginDialog = new LoginDialog();
   connect(mLoginDialog, SIGNAL(onLoginClicked()), this, SLOT(onLoginClicked()));
   connect(mLoginDialog, SIGNAL(onPreviewClicked()), this, SLOT(onPreviewClicked()));
   mLoginDialog->show();
}

std::string MegaChatApplication::getText(std::string title, bool allowEmpty)
{
    bool ok;
    std::string text;
    QString qText;

    while (1)
    {
        qText = QInputDialog::getText((QWidget *)0, tr("MEGAchat"),
                title.c_str(), QLineEdit::Normal, "", &ok);

        if (ok)
        {
            text = qText.toStdString();
            if (text.size() > 0 || allowEmpty)
            {
                return text;
            }
        }
        else
        {
            return std::string();
        }
    }
}

void MegaChatApplication::onPreviewClicked()
{
    std::string chatLink = getText("Enter chat link:");
    if (!chatLink.size())
        return;

    if (!initAnonymous(chatLink))
    {
        mLoginDialog->setState(LoginDialog::LoginStage::badCredentials);
    }
}

bool MegaChatApplication::initAnonymous(std::string chatlink)
{
    if (chatlink.size() < 43)
    {
        return false;
    }

    if (mMegaChatApi->initAnonymous() != MegaChatApi::INIT_ANONYMOUS)
    {
        mMegaChatApi->logout();
        return false;
    }

    if (mLoginDialog)
    {
        mLoginDialog->deleteLater();
        mLoginDialog = NULL;
    }
    delete [] mSid;
    mSid = NULL;

    mMainWin->setWindowTitle("Anonymous mode");
    mMegaChatApi->connect();
    mMegaChatApi->openChatPreview(chatlink.c_str());
    connect(mMainWin, SIGNAL(onAnonymousLogout()), this, SLOT(onAnonymousLogout()));
    mMainWin->show();
    mMainWin->activeControls(false);
    return true;
}

void MegaChatApplication::onAnonymousLogout()
{
    if (mMainWin)
    {
       mMainWin->deleteLater();
       mMainWin = NULL;
    }
    mMegaChatApi->logout();
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
    }
    mMegaApi->login(email.toUtf8().constData(), password.toUtf8().constData());
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
    if (userList)
    {
        if(mMainWin)
        {
            mMainWin->addOrUpdateContactControllersItems(userList);
            mMainWin->reorderAppContactList();
        }

        // if notification settings have changed for our own user, update the value
        for (int i = 0; i < userList->size(); i++)
        {
            ::mega::MegaUser *user = userList->get(i);
            if (user->getHandle() == mMegaApi->getMyUserHandleBinary())
            {
                if (user->hasChanged(MegaUser::CHANGE_TYPE_PUSH_SETTINGS) && !user->isOwnChange())
                {
                    mMegaApi->getPushNotificationSettings();
                }

                if (user->hasChanged(MegaUser::CHANGE_TYPE_ALIAS))
                {
                    mMegaApi->getUserAttribute(::mega::MegaApi::USER_ATTR_ALIAS);
                }
            }
            else
            {
                if (user->hasChanged(MegaUser::CHANGE_TYPE_FIRSTNAME))
                {
                    getFirstname(user->getHandle(), NULL, true);
                }
            }

            if (user->hasChanged(MegaUser::CHANGE_TYPE_EMAIL))
            {
                if (user->getHandle() == mMegaChatApi->getMyUserHandle())
                {
                     mMainWin->setWindowTitle(QString(user->getEmail()));
                     mMainWin->updateToolTipMyInfo();
                }
                else
                {
                    if (!getLocalUserAlias(user->getHandle()).empty() && !getFirstname(user->getHandle(), nullptr))
                    {
                       // Update contact title and messages
                       mMainWin->updateContactTitle(user->getHandle(), user->getEmail());
                       mMainWin->updateMessageFirstname(user->getHandle(), user->getEmail());
                    }
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

    delete [] chat;
    delete [] msgid;
}

const char *MegaChatApplication::getFirstname(MegaChatHandle uh, const char *authorizationToken, bool force)
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
    if (it != mFirstnamesMap.end() && !force)
    {
        return MegaApi::strdup(it->second.c_str());
    }

    if (!mFirstnameFetching[uh])
    {
        mMegaChatApi->getUserFirstname(uh, authorizationToken);
        mFirstnameFetching[uh] = true;
    }

    return NULL;
}

bool MegaChatApplication::isStagingEnabled()
{
    return useStaging;
}

void MegaChatApplication::enableStaging(bool enable)
{
    if (useStaging == enable)
    {
        return;
    }

    useStaging = enable;
    if (enable)
    {
        mMegaApi->changeApiUrl("https://staging.api.mega.co.nz/");
    }
    else
    {
        mMegaApi->changeApiUrl("https://g.api.mega.co.nz/");
    }

    if (mMegaChatApi->getOnlineStatus() != MegaChatApi::DISCONNECTED)
    {
        // force a reload upon api-url changes
        mMegaApi->fastLogin(mSid);
        mMegaChatApi->refreshUrl();
    }
}

shared_ptr<::mega::MegaPushNotificationSettings> MegaChatApplication::getNotificationSettings() const
{
    return mNotificationSettings;
}

shared_ptr<::mega::MegaTimeZoneDetails> MegaChatApplication::getTimeZoneDetails() const
{
    return mTimeZoneDetails;
}

MainWindow *MegaChatApplication::mainWindow() const
{
    return mMainWin;
}

::mega::MegaApi *MegaChatApplication::megaApi() const
{
    return mMegaApi;
}

megachat::MegaChatApi *MegaChatApplication::megaChatApi() const
{
    return mMegaChatApi;
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

std::string MegaChatApplication::base64ToBinary(const char *base64)
{
    std::string result;
    if (!base64)
        return result;

    unsigned char* bin;
    size_t binSize;
    MegaApi::base64ToBinary(base64, &bin, &binSize);
    result = std::string(reinterpret_cast<char*>(bin), binSize);
    delete [] bin;
    return result;
}

std::string MegaChatApplication::getLocalUserAlias(MegaChatHandle uh)
{
    std::string alias;
    std::map<MegaChatHandle, std::string>::iterator it = mAliasesMap.find(uh);
    if (it != mAliasesMap.end())
    {
        alias = it->second;
    }
    return alias;
}

LoginDialog *MegaChatApplication::loginDialog() const
{
    return mLoginDialog;
}

void MegaChatApplication::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    int reqType = request->getType();
    int error = e->getErrorCode();
    if (error != API_OK
            && (reqType != MegaRequest::TYPE_LOGIN || error != MegaError::API_EMFAREQUIRED)
            && (reqType != MegaRequest::TYPE_GET_ATTR_USER || error != MegaError::API_ENOENT))
    {
        QMessageBox::critical(nullptr, tr("SDK Request failed: ").append(request->getRequestString()), tr("Error: ").append(e->getErrorString()));
    }

    switch (reqType)
    {
        case MegaRequest::TYPE_LOGIN:
            if (error == MegaError::API_EMFAREQUIRED)
            {
                std::string auxcode = mMainWin->getAuthCode();
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
            else if (error == MegaError::API_OK)
            {
                if(!mMainWin->isVisible())
                {
                    resetLoginDialog();
                    mMainWin->show();
                }

                if (!mSid)
                {
                    mSid = mMegaApi->dumpSession();
                    saveSid(mSid);
                }

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
            if (error == MegaError::API_OK)
            {
                if (!mSid)
                {
                    mSid = mMegaApi->dumpSession();
                    saveSid(mSid);
                }

                mMegaApi->getPushNotificationSettings();
                mMegaApi->fetchTimeZone();

                if (mMegaChatApi->getConnectionState() == MegaChatApi::DISCONNECTED)
                {
                    mMegaChatApi->connect();
                }
            }
            else
            {
                QMessageBox::critical(nullptr, tr("Fetch Nodes"), tr("Error Fetching nodes: ").append(e->getErrorString()));
                init();
            }
            break;
        case MegaRequest::TYPE_REMOVE_CONTACT:
            if (error != MegaError::API_OK)
                QMessageBox::critical(nullptr, tr("Remove contact"), tr("Error removing contact: ").append(e->getErrorString()));
            break;

        case MegaRequest::TYPE_INVITE_CONTACT:
            if (error != MegaError::API_OK)
                QMessageBox::critical(nullptr, tr("Invite contact"), tr("Error inviting contact: ").append(e->getErrorString()));
            break;

        case MegaRequest::TYPE_MULTI_FACTOR_AUTH_CHECK:
            if (error == MegaError::API_OK)
            {
                mMainWin->createFactorMenu(request->getFlag());
            }
            break;

        case MegaRequest::TYPE_MULTI_FACTOR_AUTH_GET:
            if (error == MegaError::API_OK)
            {
                QClipboard *clipboard = QApplication::clipboard();
                QMessageBox msg;
                msg.setIcon(QMessageBox::Information);
                msg.setWindowTitle("2FA Code:");
                msg.setText(request->getText());

                QAbstractButton *copyButton = msg.addButton(tr("Copy to clipboard"), QMessageBox::ActionRole);
                copyButton->disconnect();
                connect(copyButton, &QAbstractButton::clicked, this, [=](){clipboard->setText(request->getText());});
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
        {
            QString text;
            if (request->getFlag())
            {
                text.append("Enable 2FA");
            }
            else
            {
                text.append("Disable 2FA");
            }

            if (error == MegaError::API_OK)
            {
                QMessageBox::information(nullptr, tr(text.toStdString().c_str()), tr("The operation has been completed successfully"));
            }
            else
            {
                QMessageBox::critical(nullptr, tr(text.toStdString().c_str()), tr(" ").append(e->getErrorString()));
            }
            break;
        }
        case MegaRequest::TYPE_GET_ATTR_USER:
            if (request->getParamType() == ::mega::MegaApi::USER_ATTR_PUSH_SETTINGS)
            {
                const ::mega::MegaPushNotificationSettings *currentSettings = request->getMegaPushNotificationSettings();

                if (currentSettings)
                {
                    mNotificationSettings.reset(currentSettings->copy());
                }
                else
                {
                    mNotificationSettings.reset(::mega::MegaPushNotificationSettings::createInstance());
                }

                if (mMainWin && mMainWin->mSettings && mTimeZoneDetails)
                {
                    mMainWin->mSettings->onPushNotificationSettingsUpdate();
                }
            }
            else if (request->getParamType() == ::mega::MegaApi::USER_ATTR_ALIAS)
            {
                mAliasesMap.clear();
                MegaStringMap *aliasesmap = request->getMegaStringMap();
                if (!aliasesmap)
                {
                    mMainWin->reorderAppContactList();
                    break;
                }

                std::unique_ptr<MegaStringList> keys(aliasesmap->getKeys());
                for (int i = 0; i < keys->size(); i++)
                {
                    const char *key = keys->get(i);
                    MegaChatHandle uhBin = mMegaApi->base64ToUserHandle(key);
                    std::string aliasBin = base64ToBinary(aliasesmap->get(key));
                    if (!aliasBin.empty())
                    {
                        mAliasesMap[uhBin] = aliasBin;
                    }
                }
                mMainWin->reorderAppContactList();
            }
            break;
        case MegaRequest::TYPE_FETCH_TIMEZONE:
        {
            mTimeZoneDetails.reset(request->getMegaTimeZoneDetails()->copy());

            if (mMainWin && mMainWin->mSettings && mNotificationSettings)
            {
                mMainWin->mSettings->onPushNotificationSettingsUpdate();
            }
        }
        break;
        case MegaRequest::TYPE_SET_ATTR_USER:
        {
        }
        break;

        default:
            break;
    }
}

void MegaChatApplication::onRequestFinish(MegaChatApi *, MegaChatRequest *request, MegaChatError *e)
{
    int reqType = request->getType();
    int error = e->getErrorCode();
    if (error != MegaChatError::ERROR_OK)
    {
        QMessageBox::critical(nullptr, tr("MEGAchat Request failed: ").append(request->getRequestString()), tr("Error: ").append(e->getErrorString()));
    }

    switch (reqType)
    {
         case MegaChatRequest::TYPE_CONNECT:
            if (error != MegaChatError::ERROR_OK)
            {
                QMessageBox::critical(nullptr, tr("Chat Connection"), tr("Error stablishing connection: ").append(e->getErrorString()));
                init();
            }
            else
            {
                MegaUserList *contactList = mMegaApi->getContacts();
                mMainWin->addOrUpdateContactControllersItems(contactList);
                mMainWin->reorderAppContactList();
                //Fetch alias attr
                mMegaApi->getUserAttribute(::mega::MegaApi::USER_ATTR_ALIAS);
                delete contactList;
            }
            break;
          case MegaChatRequest::TYPE_LOGOUT:
            if (error == MegaChatError::ERROR_OK)
            {
                removeSid();
                init();
            }
            break;
         case MegaChatRequest::TYPE_GET_FIRSTNAME:
             {
             MegaChatHandle userHandle = request->getUserHandle();
             int errorCode = error;
             mFirstnameFetching[userHandle] = false;
             if (errorCode == MegaChatError::ERROR_OK)
             {
                const char *firstname = request->getText();
                if ((strlen(firstname)) == 0)
                {
                    mFirstnamesMap.erase(userHandle);
                    mMegaChatApi->getUserEmail(userHandle);
                    break;
                }
                mFirstnamesMap[userHandle] = firstname;
                mMainWin->updateContactTitle(userHandle,firstname);
                mMainWin->updateMessageFirstname(userHandle,firstname);
             }
             else if (errorCode == MegaChatError::ERROR_NOENT)
             {
                mFirstnamesMap.erase(userHandle);
                this->mMegaChatApi->getUserEmail(userHandle);
             }
             break;
             }
         case MegaChatRequest::TYPE_GET_EMAIL:
            {
            MegaChatHandle userHandle = request->getUserHandle();
            if (error == MegaChatError::ERROR_OK)
            {
               const char *email = request->getText();
               if (userHandle == mMegaChatApi->getMyUserHandle())
               {
                    mMainWin->setWindowTitle(QString(email));
                    mMainWin->updateToolTipMyInfo();
               }
               else
               {
                  if (mFirstnamesMap.find(userHandle) == mFirstnamesMap.end()
                          && getLocalUserAlias(userHandle).empty())
                  {
                     // Update contact title and messages
                     mMainWin->updateContactTitle(userHandle, email);
                     mMainWin->updateMessageFirstname(userHandle, email);
                  }
                  else
                  {
                      std::map<megachat::MegaChatHandle, ChatListItemController *>::iterator it;
                      for (it = mMainWin->mChatControllers.begin(); it != mMainWin->mChatControllers.end(); it++)
                      {
                          ChatListItemController *itemController = it->second;
                          const MegaChatListItem *item = itemController->getItem();
                          ChatItemWidget *widget = itemController->getWidget();
                          if (item && widget)
                          {
                              widget->updateToolTip(item);
                          }
                      }
                  }
               }
            }
            else
            {
               mMainWin->updateMessageFirstname(userHandle,"Unknown contact");
            }
            break;
            }
         case MegaChatRequest::TYPE_CREATE_CHATROOM:
             if (error == MegaChatError::ERROR_OK)
             {
                MegaChatHandle chatid = request->getChatHandle();
                const MegaChatListItem *chatListItem = mMegaChatApi->getChatListItem(chatid);

                if (!chatListItem->isGroup())
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
            if (error != MegaChatError::ERROR_OK)
                QMessageBox::critical(nullptr, tr("Leave chat"), tr("Error leaving chat: ").append(e->getErrorString()));
            break;
         case MegaChatRequest::TYPE_EDIT_CHATROOM_NAME:
            if (error != MegaChatError::ERROR_OK)
                QMessageBox::critical(nullptr, tr("Edit chat topic"), tr("Error modifiying chat topic: ").append(e->getErrorString()));
            break;

        case MegaChatRequest::TYPE_ARCHIVE_CHATROOM:
            if (error != MegaChatError::ERROR_OK)
            {
                QMessageBox::critical(nullptr, tr("Archive chat"), tr("Error archiving chat: ").append(e->getErrorString()));
            }
            break;

#ifndef KARERE_DISABLE_WEBRTC
         case MegaChatRequest::TYPE_ANSWER_CHAT_CALL:
         case MegaChatRequest::TYPE_START_CHAT_CALL:
            if (error != MegaChatError::ERROR_OK)
              {
                QMessageBox::critical(nullptr, tr("Call"), tr("Error in call: ").append(e->getErrorString()));
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
            else
            {
                megachat::MegaChatHandle chatId = request->getChatHandle();
                ChatListItemController *itemController = mMainWin->getChatControllerById(chatId);
                if(itemController)
                {
                    ChatWindow *chatWin = itemController->showChatWindow();
                    chatWin->connectPeerCallGui(mMegaChatApi->getMyUserHandle(), mMegaChatApi->getMyClientidHandle(chatId));
                }
            }
            break;

          case MegaChatRequest::TYPE_HANG_CHAT_CALL:
            if (error != MegaChatError::ERROR_OK)
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
            if (error != MegaChatError::ERROR_OK)
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

            //chat links
            case MegaChatRequest::TYPE_CHAT_LINK_HANDLE:
                {
                    bool del = request->getFlag();
                    if (del)
                    {
                        if(error != MegaChatError::ERROR_OK)
                        {
                            QMessageBox::critical(nullptr, tr("Remove chat link"), tr("Error removing the chat link: ").append(e->getErrorString()));
                        }
                        else
                        {
                            QMessageBox::information(nullptr, tr("Remove chat link"), tr("The chat link has been removed"));
                        }
                    }
                    else
                    {
                        QMessageBox msg;
                        msg.setIcon(QMessageBox::Information);

                        bool createifmissing = request->getNumRetry();
                        if (createifmissing)
                        {
                            if(error == MegaChatError::ERROR_OK)
                            {
                                msg.setText("The chat link has been generated successfully");
                            }
                            else
                            {
                                QMessageBox::critical(nullptr, tr("Create chat link"), tr("Error exporting chat link: ").append(e->getErrorString()));
                            }
                        }
                        else
                        {
                            if(error == MegaChatError::ERROR_OK)
                            {
                                msg.setText("The chat link already exists");
                            }
                            else if (error == MegaChatError::ERROR_NOENT)
                            {
                                QMessageBox::warning(nullptr, tr("Query chat link"), tr("No chat-link exists in this chatroom"));
                            }
                            else
                            {
                                QMessageBox::critical(nullptr, tr("Query chat link"), tr("Error querying chat link: ").append(e->getErrorString()));
                            }
                        }

                        if(error == MegaChatError::ERROR_OK)
                        {
                            QString chatlink (request->getText());
                            msg.setDetailedText(chatlink);
                            foreach (QAbstractButton *button, msg.buttons())
                            {
                                if (msg.buttonRole(button) == QMessageBox::ActionRole)
                                {
                                    button->click();
                                    break;
                                }
                            }
                            msg.exec();
                        }
                    }
                }
                break;

            case MegaChatRequest::TYPE_LOAD_PREVIEW:
            {
                MegaChatHandle chatid = request->getChatHandle();
                bool checkChatLink = !request->getFlag();

                //Check chat link
                if (checkChatLink)
                {
                    if (error == MegaChatError::ERROR_OK)
                    {
                        int numPeers = static_cast<int>(request->getNumber());
                        const char *title = request->getText();
                        const char *chatHandle_64 = mMegaApi->userHandleToBase64(chatid);

                        QString line = QString("%1: \n\n Chatid: %2 \n Title: %3 \n Participants: %4 \n\n Do you want to preview it?")
                                .arg(QString::fromStdString(request->getLink()))
                                .arg(QString::fromStdString(chatHandle_64))
                                .arg(QString(title))
                                .arg(QString::fromStdString(std::to_string(numPeers)));

                        QMessageBox msgBox;
                        msgBox.setWindowTitle("Check chat link");
                        msgBox.setText(line);
                        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                        msgBox.setDefaultButton(QMessageBox::Cancel);
                        int ret = msgBox.exec();
                        if (ret == QMessageBox::Ok)
                        {
                            mMegaChatApi->openChatPreview(request->getLink());
                        }

                        delete [] chatHandle_64;
                    }
                    else
                    {
                        QMessageBox::critical(nullptr, tr("Check chat link"), tr("Error checking a chat-link: ").append(e->getErrorString()));
                    }
                }
                else //Load chat link
                {
                    if (error == MegaChatError::ERROR_OK)
                    {
                        const MegaChatListItem *chatListItem = mMegaChatApi->getChatListItem(chatid);
                        mMainWin->addOrUpdateChatControllerItem(chatListItem->copy());
                        mMainWin->reorderAppChatList();
                        ChatWindow *auxWin = mMainWin->getChatWindowIfExists(request->getChatHandle());
                        if (auxWin)
                        {
                            auxWin->previewUpdate(mMegaChatApi->getChatRoom(request->getChatHandle()));
                        }
                        delete chatListItem;
                    }
                    else
                    {
                        MegaChatRoom *room = mMegaChatApi->getChatRoom(chatid);
                        if (room)
                        {
                            if (room->isPreview() && room->isActive())
                            {
                                QMessageBox::critical(nullptr, tr("Load chat link"), tr("You are trying to open a chat in preview mode twice"));
                            }
                            else if(room->isActive())
                            {
                                QMessageBox::critical(nullptr, tr("Load chat link"), tr("You are trying to preview a chat wich you are currently part of"));
                            }
                            else //Rejoin
                            {
                                QMessageBox msgBox;
                                msgBox.setText("You are trying to preview a chat which you were part of. Do you want to rejoin this chat?");
                                msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                                msgBox.setDefaultButton(QMessageBox::Cancel);
                                int ret = msgBox.exec();
                                if (ret == QMessageBox::Ok)
                                {
                                    MegaChatHandle ph = request->getUserHandle();
                                    mMegaChatApi->autorejoinPublicChat(chatid, ph);
                                }
                            }
                            delete room;
                        }
                        else
                        {
                            QMessageBox::critical(nullptr, tr("Load chat link"), tr("Error loading chat link"));
                        }
                    }
                }
                break;
            }

            case MegaChatRequest::TYPE_SET_PRIVATE_MODE:
            {
                if(error != MegaChatError::ERROR_OK)
                {
                    QMessageBox::critical(nullptr, tr("Close chat link"), tr("Error setting chat into private mode: ").append(e->getErrorString()));
                }
                else
                {
                    QMessageBox::information(nullptr, tr("Close chat link"), tr("The chat has been converted into private mode"));
                }
                break;
            }

            case MegaChatRequest::TYPE_AUTOJOIN_PUBLIC_CHAT:
            {
                if(error == MegaChatError::ERROR_OK)
                {
                    const MegaChatListItem *chatListItem = mMegaChatApi->getChatListItem(request->getChatHandle());
                    mMainWin->addOrUpdateChatControllerItem(chatListItem->copy());
                    mMainWin->reorderAppChatList();

                    ChatWindow *auxWin = mMainWin->getChatWindowIfExists(request->getChatHandle());
                    if (auxWin)
                    {
                        auxWin->previewUpdate(mMegaChatApi->getChatRoom(request->getChatHandle()));
                    }
                    delete chatListItem;

                    if (request->getUserHandle() == megachat::MEGACHAT_INVALID_HANDLE)
                    {
                        QMessageBox::information(nullptr, tr("Join chat link"), tr("You have joined successfully"));
                    }
                    else
                    {
                        QMessageBox::information(nullptr, tr("Rejoin chat link"), tr("You have rejoined successfully"));
                    }
                }
                else
                {
                    QMessageBox::critical(nullptr, tr("Join chat link"), tr("Error joining chat link: ").append(e->getErrorString()));
                }
                break;
            }
    default:
        break;
    }
}
