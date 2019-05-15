#include "listItemController.h"

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
