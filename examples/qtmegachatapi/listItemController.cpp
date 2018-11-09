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

void ChatListItemController::setChatWindow(ChatWindow *chatWindow)
{
    mChatWindow = chatWindow;
}

ChatItemWidget *ChatListItemController::getWidget() const
{
    return mWidget;
}

void ChatListItemController::setWidget(ChatItemWidget *widget)
{
    mWidget = widget;
}

megachat::MegaChatListItem *ChatListItemController::getItem() const
{
    return mItem;
}

void ChatListItemController::setItem(megachat::MegaChatListItem *item)
{
    mItem = item;
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

void ContactListItemController::updateWidget(ContactItemWidget *widget)
{
    if (mWidget)
    {
        delete mWidget;
    }

    mWidget = widget;
}

void ContactListItemController::updateItem(mega::MegaUser *item)
{
    if (mItem)
    {
        delete mItem;
    }
    mItem = item;
}
