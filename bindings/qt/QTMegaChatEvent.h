#ifndef QTMEGACHATEVENT_H
#define QTMEGACHATEVENT_H

#include <megachatapi.h>
#include <QEvent>

namespace megachat
{

class QTMegaChatEvent: public QEvent
{
public:
    enum MegaType
    {
        OnRequestStart = QEvent::User + 200,
        OnRequestUpdate,
        OnRequestFinish,
        OnRequestTemporaryError,
        OnChatListItemUpdate,
        OnChatInitStateUpdate,
        OnChatOnlineStatusUpdate,
        OnChatPresenceConfigUpdate,
        OnChatConnectionStateUpdate,
        OnChatRoomUpdate,
        OnMessageLoaded,
        OnMessageReceived,
        OnMessageUpdate
    };

    QTMegaChatEvent(MegaChatApi *megaChatApi, Type type);
    ~QTMegaChatEvent();

    MegaChatApi *getMegaChatApi();
    MegaChatRequest *getChatRequest();
    MegaChatError *getChatError();
    MegaChatListItem *getChatListItem();
    MegaChatHandle getChatHandle();
    MegaChatPresenceConfig *getPresenceConfig();
    MegaChatRoom *getChatRoom();
    MegaChatMessage *getChatMessage();
    bool getProgress();
    int getStatus();

    void setChatRequest(MegaChatRequest *request);
    void setChatError(MegaChatError *error);
    void setChatListItem(MegaChatListItem *item);
    void setChatHandle(MegaChatHandle handle);
    void setPresenceConfig(MegaChatPresenceConfig *config);
    void setChatRoom(MegaChatRoom *chat);
    void setChatMessage(MegaChatMessage *msg);
    void setProgress(bool progress);
    void setStatus(int status);

private:
    MegaChatApi *megaChatApi;
    MegaChatRequest *request;
    MegaChatError *error;
    MegaChatListItem *item;
    MegaChatHandle handle;
    MegaChatPresenceConfig *config;
    MegaChatRoom *chat;
    MegaChatMessage *msg;
    bool inProgress;
    int status;
};

}

#endif // QTMEGACHATEVENT_H
