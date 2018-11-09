#ifndef LISTITEMCONTROLLER_H
#define LISTITEMCONTROLLER_H
#include "megachatapi.h"
#include "chatWindow.h"

class ChatWindow;
class ChatItemWidget;
class ContactItemWidget;

class ListItemController
{
    public:
        virtual megachat::MegaChatHandle getItemId() const = 0;

    protected:
        megachat::MegaChatHandle mItemId = megachat::MEGACHAT_INVALID_HANDLE;
};

class ChatListItemController :
    public ListItemController
{
    public:
        ChatListItemController(megachat::MegaChatListItem *item, ChatItemWidget *widget, ChatWindow *chatWindow = NULL);
        ~ChatListItemController();
        ChatItemWidget *getWidget() const;
        megachat::MegaChatListItem *getItem() const;
        megachat::MegaChatHandle getItemId() const;
        ChatWindow *getChatWindow() const;
        void setWidget(ChatItemWidget *widget);
        void setItem(megachat::MegaChatListItem *item);
        void setChatWindow(ChatWindow *window);
    protected:
        megachat::MegaChatListItem *mItem = nullptr;
        ChatItemWidget *mWidget = nullptr;
        ChatWindow *mChatWindow = nullptr;
};

class ContactListItemController:
        public ListItemController
{
    public:
        ContactListItemController(mega::MegaUser *item, ContactItemWidget *widget);
        ~ContactListItemController();
        ContactItemWidget *getWidget() const;
        mega::MegaUser *getItem() const;
        megachat::MegaChatHandle getItemId() const;
        void setWidget(ContactItemWidget *widget);
        void setItem(mega::MegaUser *item);
    protected:
        mega::MegaUser *mItem = nullptr;
        ContactItemWidget *mWidget = nullptr;
};
#endif // LISTITEMCONTROLLER_H
