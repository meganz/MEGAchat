#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>
#include <QDateTime>
#include <QListWidgetItem>
#include "megachatapi.h"
#include "ui_chatMessageWidget.h"
#include "chatWindow.h"
class ChatWindow;
namespace Ui {
class ChatMessageWidget;
}

class ChatMessage: public QWidget
{
    Q_OBJECT
    protected:
        Ui::ChatMessageWidget *ui;
        megachat::MegaChatHandle chatId;
        megachat::MegaChatMessage *message;
        megachat::MegaChatApi* megaChatApi;
        QListWidgetItem * widgetItem;
        void updateToolTip();
        ChatWindow *chatWin;
        bool edited;
        friend class ChatWindow;
    public:
        ChatMessage(ChatWindow *parent, megachat::MegaChatApi* mChatApi, megachat::MegaChatHandle chatId, megachat::MegaChatMessage *msg);
        std::string managementInfoToString() const;
        void setMessageContent(const char * content);
        void setTimestamp(int64_t ts);
        void setStatus(int status);
        void setAuthor();
        void markAsEdited();
        virtual ~ChatMessage();
        QListWidgetItem *getWidgetItem() const;
        void setWidgetItem(QListWidgetItem *item);
        bool isMine() const;
        megachat::MegaChatMessage *getMessage() const;
        void setMessage(megachat::MegaChatMessage *message);
        void startEditingMsgWidget();
        ChatMessage* clearEdit();

    public slots:
        void cancelMsgEdit(bool clicked);
        void saveMsgEdit(bool clicked);
        void onMessageCtxMenu(const QPoint& point);
        void onMessageDelAction();
        void onMessageEditAction();
};
#endif // CHATMESSAGE_H
