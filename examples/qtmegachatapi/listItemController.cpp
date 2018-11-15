#include "listItemController.h"

ChatListItemController::ChatListItemController(megachat::MegaChatListItem *item, ChatItemWidget *widget, ChatWindow *chatWindow)
{
    mItemId = item->getChatId();
    mItem = item;
    mWidget = widget;
    mChatWindow = chatWindow;
}

ChatListItemController::~ChatListItemController()
{
    delete mChatWindow;
    delete mWidget;
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
        delete mChatWindow;
    }

    mChatWindow = chatWindow;
}

void ChatListItemController::addOrUpdateWidget(ChatItemWidget *widget)
{
    if (mWidget)
    {
        delete mWidget;
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

ChatWindow* ChatListItemController::showChatWindow()
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

ContactListItemController::ContactListItemController(mega::MegaUser *item, ContactItemWidget *widget)
{
    mItemId = item->getHandle();
    mItem = item;
    mWidget = widget;
}

ContactListItemController::~ContactListItemController()
{
    delete mWidget;
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
        delete mWidget;
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
