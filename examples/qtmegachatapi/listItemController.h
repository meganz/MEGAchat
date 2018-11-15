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
        ChatListItemController(megachat::MegaChatListItem *item, ChatItemWidget *widget = nullptr, ChatWindow *chatWindow = nullptr);
        ~ChatListItemController();
        ChatItemWidget *getWidget() const;
        megachat::MegaChatListItem *getItem() const;
        megachat::MegaChatHandle getItemId() const;
        ChatWindow *getChatWindow() const;
        void addOrUpdateWidget(ChatItemWidget *widget);
        void addOrUpdateItem(megachat::MegaChatListItem *item);
        ChatWindow* showChatWindow();
        void addOrUpdateChatWindow(ChatWindow *window);
    protected:
        megachat::MegaChatListItem *mItem = nullptr;
        ChatItemWidget *mWidget = nullptr;
        ChatWindow *mChatWindow = nullptr;
};

class ContactListItemController:
        public ListItemController
{
    public:
        ContactListItemController(mega::MegaUser *item, ContactItemWidget *widget = nullptr);
        ~ContactListItemController();
        ContactItemWidget *getWidget() const;
        mega::MegaUser *getItem() const;
        megachat::MegaChatHandle getItemId() const;

        /* Update the widget in contactController. The ContactListItemController
         * retains the ownership of the ContactItemWidget */
        void addOrUpdateWidget(ContactItemWidget *widget);

        /* Update the item in contactController. The ContactListItemController
         * retains the ownership of the mega::MegaUser */
        void addOrUpdateItem(mega::MegaUser *item);
    protected:
        mega::MegaUser *mItem = nullptr;
        ContactItemWidget *mWidget = nullptr;
};
#endif // LISTITEMCONTROLLER_H
