#include "listItemController.h"
#include <mega/utils.h>
#include <QInputDialog>

using namespace megachat;

ChatListItemController::ChatListItemController(MainWindow *mainWindow, megachat::MegaChatListItem *item, ChatItemWidget *widget, ChatWindow *chatWindow)
    : QObject(mainWindow),
      ListItemController(item->getChatId()),
      mItem(item),
      mWidget(widget),
      mChatWindow(chatWindow),
      mMainWindow(mainWindow),
      mMegaChatApi(mainWindow->mMegaChatApi),
      mMegaApi(mainWindow->mMegaApi)
{
}

ChatListItemController::~ChatListItemController()
{
    mChatWindow->deleteLater();
    mWidget->deleteLater();
    delete mItem;
}

megachat::MegaChatHandle ChatListItemController::getItemId() const
{
    return mItemId;
}

ChatWindow *ChatListItemController::getChatWindow() const
{
    return mChatWindow;
}

ChatItemWidget *ChatListItemController::getWidget() const
{
    return mWidget;
}

megachat::MegaChatListItem *ChatListItemController::getItem() const
{
    return mItem;
}

void ChatListItemController::addOrUpdateChatWindow(ChatWindow *chatWindow)
{
    if (mChatWindow)
    {
        mChatWindow->deleteLater();
    }

    mChatWindow = chatWindow;
}

void ChatListItemController::invalidChatWindow()
{
    mChatWindow = nullptr;
}

#ifndef KARERE_DISABLE_WEBRTC
void ChatListItemController::createMeetingView()
{
    assert(!mMeetingView);
    mMeetingView = new MeetingView(*mMegaChatApi, mItem->getChatId(), mMainWindow);
    mMeetingView->show();
}

void ChatListItemController::destroyMeetingView()
{
    assert(mMeetingView);
    mMeetingView->deleteLater();
    mMeetingView = nullptr;
}

MeetingView* ChatListItemController::getMeetingView()
{
    return mMeetingView;
}
#endif

void ChatListItemController::addOrUpdateWidget(ChatItemWidget *widget)
{
    if (mWidget)
    {
        mWidget->deleteLater();
    }
    mWidget = widget;
}

void ChatListItemController::addOrUpdateItem(megachat::MegaChatListItem *item)
{
    if (mItem)
    {
        delete mItem;
    }
    mItem = item;
}

ChatWindow *ChatListItemController::showChatWindow()
{
    if (!mChatWindow)
    {
        megachat::MegaChatRoom *chatRoom = mWidget->mMegaChatApi->getChatRoom(mItemId);
        mChatWindow = new ChatWindow(mWidget->mMainWin, mWidget->mMegaChatApi, chatRoom->copy(), mItem->getTitle());
        mChatWindow->show();
        mChatWindow->openChatRoom();
        delete chatRoom;
    }
    else
    {
        mChatWindow->show();
        mChatWindow->setWindowState(Qt::WindowActive);
    }
    return mChatWindow;
}

void ChatListItemController::leaveGroupChat()
{
    mMegaChatApi->leaveChat(mItemId);
}

void ChatListItemController::updateScheduledMeetingOccurrence()
{
    MegaChatHandle schedId = mMegaApi->base64ToUserHandle(mMainWindow->mApp->getText("Sched Id of occurrence we want to modify (B64): ", false).c_str());
    MegaChatTimeStamp overrides = atoi(mMainWindow->mApp->getText("Start date we want to modify (Unix timestamp)", true).c_str());
    MegaChatTimeStamp newStartDate = atoi(mMainWindow->mApp->getText("New start date (Unix timestamp)", true).c_str());
    MegaChatTimeStamp newEndDate = atoi(mMainWindow->mApp->getText("New end date (Unix timestamp)", true).c_str());
    int cancelled = atoi(mMainWindow->mApp->getText("Set occurrence as cancelled? Y=1 | N =0", true).c_str());

    mMegaChatApi->updateScheduledMeetingOccurrence(mItemId,
                                                   schedId,
                                                   overrides,
                                                   newStartDate,
                                                   newEndDate,
                                                   cancelled == 1 ? true : false /*cancelled*/);
}

void ChatListItemController::updateScheduledMeeting()
{
    // this action won't generate a child scheduled meeting
    const std::string updateChatTitle = mMainWindow->mApp->getText("Update chatroom title [y/n]: ", true);
    std::string schedB64 = mMainWindow->mApp->getText("Sched meeting Id we want to modify: ", true);
    MegaChatHandle schedId = mMegaApi->base64ToUserHandle(schedB64.c_str());
    std::unique_ptr<MegaChatScheduledMeeting> sm (mMegaChatApi->getScheduledMeeting(mItemId, schedId));
    if (!sm) { return; }

    std::string newTitle = mMainWindow->mApp->getText("New title: ", true);
    std::string newDesc = mMainWindow->mApp->getText("New decription: ", true);
    MegaChatTimeStamp newStartDate = atoi(mMainWindow->mApp->getText("New StartDate: ", true).c_str());
    MegaChatTimeStamp newEndDate = atoi(mMainWindow->mApp->getText("New EndDate: ", true).c_str());
    std::string newTz = mMainWindow->mApp->getText("New TimeZone: ", true);
    int cancelled = atoi(mMainWindow->mApp->getText("Set scheduled meeting as cancelled? Y=1 | N =0", true).c_str());

    mMegaChatApi->updateScheduledMeeting(mItemId, schedId,
                                         newTz.empty() ? nullptr : newTz.c_str(),
                                         newStartDate,
                                         newEndDate,
                                         newTitle.empty() ? nullptr : newTitle.c_str(),
                                         newDesc.empty() ? nullptr : newDesc.c_str(),
                                         cancelled, sm->flags(), sm->rules(),
                                         updateChatTitle == "y");
}
void ChatListItemController::removeScheduledMeeting()
{
    std::string aux = mMainWindow->mApp->getText("Sched meeting Id to remove: ", false).c_str();
    uint64_t schedId = mMegaApi->base64ToUserHandle(aux.c_str());
    mMegaApi->removeScheduledMeeting(mItemId, schedId);
}

void ChatListItemController::fetchScheduledMeeting()
{
    mMegaApi->fetchScheduledMeeting(mItemId, MEGACHAT_INVALID_HANDLE);
}

void ChatListItemController::fetchScheduledMeetingEvents()
{
    MegaChatTimeStamp since = atoi(mMainWindow->mApp->getText("Date from which we want to get occurrences (Unix timestamp)", true).c_str());
    mMegaChatApi->fetchScheduledMeetingOccurrencesByChat(mItemId, since);
}

void ChatListItemController::endCall()
{
#ifndef KARERE_DISABLE_WEBRTC
    std::unique_ptr<megachat::MegaChatCall> call = std::unique_ptr<megachat::MegaChatCall>(mMegaChatApi->getChatCall(mItemId));
    if (call)
    {
        mMegaChatApi->endChatCall(call->getCallId());
    }
#else
    mMainWindow->mApp->noFeatureErr();
#endif
}

void ChatListItemController::setTitle()
{
    std::string title;
    QString qTitle = QInputDialog::getText(mMainWindow, tr("Change chat topic"), tr("Leave blank for default title"));
    if (!qTitle.isNull())
    {
        title = qTitle.toStdString();
        if (title.empty())
        {
            QMessageBox::warning(mMainWindow, tr("Set chat title"), tr("You can't set an empty title"));
            return;
        }
        mMegaChatApi->setChatTitle(mItemId, title.c_str());
    }
}

void ChatListItemController::truncateChat()
{
    this->mMegaChatApi->clearChatHistory(mItemId);
}

void ChatListItemController::onGetRetentionTime()
{
    ::mega::unique_ptr <megachat::MegaChatRoom> chatRoom(mMegaChatApi->getChatRoom(mItemId));
    if (!chatRoom)
    {
        return;
    }

    QMessageBox::information(mMainWindow, tr("Retention time: "), tr("Retention time: ")
                             .append(std::to_string(chatRoom->getRetentionTime()).c_str())
                             .append(" seconds"));
}

void ChatListItemController::onSetRetentionTime()
{
    QString text = QInputDialog::getText(mMainWindow, tr("Set retention time"),
         tr("Specify retention time")
         .append(" <b>in seconds</b>")
         .append(" (0 to disable)"));

    if (!text.isNull() && !text.isEmpty())
    {
        mMegaChatApi->setChatRetentionTime(mItemId, text.toInt());
    }
}

void ChatListItemController::onGetChatOptions()
{
    ::mega::unique_ptr <megachat::MegaChatRoom> chatRoom(mMegaChatApi->getChatRoom(mItemId));
    if (!chatRoom)
    {
        return;
    }

    QMessageBox::information(mMainWindow, tr("ChatOptions"), tr(" ")
                             .append("<br />OpenInvite: ").append(chatRoom->isOpenInvite() ? "Enabled" : "Disabled")
                             .append("<br />SpeakRequest: ").append(chatRoom->isSpeakRequest() ? "Enabled" : "Disabled")
                             .append("<br />Waiting Room :").append(chatRoom->isWaitingRoom() ? "Enabled" : "Disabled"));
}
void ChatListItemController::onSetOpenInvite(bool enable)
{
    mMegaChatApi->setOpenInvite(mItemId, enable);
}

void ChatListItemController::onSetSpeakRequest(bool enable)
{
    mMegaChatApi->setSpeakRequest(mItemId, enable);
}

void ChatListItemController::onSetWaitingRoom(bool enable)
{
    mMegaChatApi->setWaitingRoom(mItemId, enable);
}

void ChatListItemController::onWaitingRoomCall()
{
#ifndef KARERE_DISABLE_WEBRTC
    /* schedId:
     * - If valid, redirect users to waiting room and don't ring
     * - If not valid, bypass waiting room and ring
     */
    std::string schedIdStr = mMainWindow->mApp->getText("Get schedId (valid: redirect wr and don't ring | invalid: bypass wr and ring)");
    MegaChatHandle schedId = schedIdStr.empty() ? MEGACHAT_INVALID_HANDLE : mMegaApi->base64ToUserHandle(schedIdStr.c_str());
    mMegaChatApi->startMeetingInWaitingRoomChat(mItemId, schedId, false, false);
#endif
}

void ChatListItemController::onAudioCallNoRingBtn()
{
#ifndef KARERE_DISABLE_WEBRTC
    std::string schedIdStr = mMainWindow->mApp->getText("Get scheduled meeting id");
    MegaChatHandle schedId = schedIdStr.empty() ? MEGACHAT_INVALID_HANDLE : mMegaApi->base64ToUserHandle(schedIdStr.c_str());
    mMegaChatApi->startChatCallNoRinging(mItemId, schedId, false, false);
#endif
}

void ChatListItemController::queryChatLink()
{
    if (mItemId != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->queryChatLink(mItemId);
    }
}

void ChatListItemController::createChatLink()
{
    if (mItemId != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->createChatLink(mItemId);
    }
}

void ChatListItemController::setPublicChatToPrivate()
{
    if (mItemId != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->setPublicChatToPrivate(mItemId);
    }
}

void ChatListItemController::closeChatPreview()
{
    mMainWindow->closeChatPreview(mItemId);
}

void ChatListItemController::removeChatLink()
{
    if (mItemId != MEGACHAT_INVALID_HANDLE)
    {
        mMegaChatApi->removeChatLink(mItemId);
    }
}

void ChatListItemController::autojoinChatLink()
{
    auto ret = QMessageBox::question(mMainWindow, tr("Join chat link"), tr("Do you want to join to this chat?"));
    if (ret != QMessageBox::Yes)
        return;

    mMegaChatApi->autojoinPublicChat(mItemId);
}

void ChatListItemController::archiveChat(bool checked)
{
    if (mItem->isArchived() != checked)
    {
        mMegaChatApi->archiveChat(mItemId, checked);
    }
}

void ChatListItemController::onCheckPushNotificationRestrictionClicked()
{
    bool pushNotification = mMegaApi->isChatNotifiable(mItemId);
    std::string result;
    const char *chatid_64 = mMegaApi->userHandleToBase64(mItemId);
    result.append("Notification for chat: ")
            .append(mItem->getTitle())
            .append(" (").append(chatid_64).append(")")
            .append(pushNotification ? " is generated" : " is NOT generated");
    delete [] chatid_64;
    if (pushNotification)
    {
        QMessageBox::information(mMainWindow, tr("PUSH notification restriction"), result.c_str());
    }
    else
    {
        QMessageBox::warning(mMainWindow, tr("PUSH notification restriction"), result.c_str());
    }
}

void ChatListItemController::onPushReceivedIos()
{
    onPushReceived(1);
}

void ChatListItemController::onPushReceivedAndroid()
{
    onPushReceived(0);
}

void ChatListItemController::onMuteNotifications(bool enabled)
{
    auto settings = mMainWindow->mApp->getNotificationSettings();
    if (settings && settings->isChatDndEnabled(mItemId) != enabled)
    {
        settings->enableChat(mItemId, !enabled);
        mMegaApi->setPushNotificationSettings(settings.get());
    }
}

void ChatListItemController::onSetAlwaysNotify(bool enabled)
{
    auto settings = mMainWindow->mApp->getNotificationSettings();
    if (settings && settings->isChatAlwaysNotifyEnabled(mItemId) != enabled)
    {
        settings->enableChatAlwaysNotify(mItemId, enabled);
        mMegaApi->setPushNotificationSettings(settings.get());
    }
}

void ChatListItemController::onSetDND()
{
    auto settings = mMainWindow->mApp->getNotificationSettings();
    if (!settings)
    {
        return;
    }

    ::mega::m_time_t now = ::mega::m_time(NULL);
    ::mega::m_time_t currentDND = settings->getChatDnd(mItemId);
    ::mega::m_time_t currentValue = (currentDND > 0) ? currentDND - now : currentDND;

    bool ok = false;
    ::mega::m_time_t newValue = QInputDialog::getInt(mMainWindow, tr("Push notification restriction - DND"),
                                                 tr("Set DND mode for this chatroom for (in seconds)"
                                                    "\n(0 to disable notifications, -1 to unset DND): "),
                                                     static_cast<int>(currentValue), -1, 2147483647, 1, &ok);
    ::mega::m_time_t newDND = (newValue > 0) ? newValue + now : newValue;
    if (ok && currentDND != newDND)
    {
        if (newDND > 0)
        {
            newDND = newValue + ::mega::m_time(NULL);   // update when the user clicks OK
            settings->setChatDnd(mItemId, newDND);
        }
        else
        {
            // -1 --> enable, 0 --> disable
            settings->enableChat(mItemId, newDND);
        }
        mMegaApi->setPushNotificationSettings(settings.get());
    }
}

void ChatListItemController::onPushReceived(unsigned int type)
{
    if (!type)
    {
        mMegaChatApi->pushReceived(false);
    }
    else
    {
        mMegaChatApi->pushReceived(false, mItemId);
    }
}

ContactListItemController::ContactListItemController(mega::MegaUser *item, ContactItemWidget *widget)
    : ListItemController(item->getHandle()),
      mItem(item),
      mWidget(widget)
{
}

ContactListItemController::~ContactListItemController()
{
    mWidget->deleteLater();
    delete mItem;
}

megachat::MegaChatHandle ContactListItemController::getItemId() const
{
    return mItemId;
}

ContactItemWidget *ContactListItemController::getWidget() const
{
    return mWidget;
}

mega::MegaUser *ContactListItemController::getItem() const
{
    return mItem;
}

void ContactListItemController::addOrUpdateWidget(ContactItemWidget *widget)
{
    if (mWidget)
    {
        mWidget->deleteLater();
    }

    mWidget = widget;
}

void ContactListItemController::addOrUpdateItem(mega::MegaUser *item)
{
    if (mItem)
    {
        delete mItem;
    }
    mItem = item;
}
