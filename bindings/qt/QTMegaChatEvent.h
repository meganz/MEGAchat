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
        OnMessageUpdate,
        OnChatVideoData,
        OnChatCallUpdate
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
    MegaChatCall *getChatCall();
    bool getProgress();
    int getStatus();
    int getWidth();
    int getHeight();
    char *getBuffer();
    size_t getSize();

    void setChatRequest(MegaChatRequest *request);
    void setChatError(MegaChatError *error);
    void setChatListItem(MegaChatListItem *item);
    void setChatHandle(MegaChatHandle handle);
    void setPresenceConfig(MegaChatPresenceConfig *config);
    void setChatRoom(MegaChatRoom *chat);
    void setChatMessage(MegaChatMessage *msg);
    void setChatCall(MegaChatCall *call);
    void setProgress(bool progress);
    void setStatus(int status);
    void setWidth(int width);
    void setHeight(int height);
    void setBuffer(char *buffer);
    void setSize(size_t size);


private:
    MegaChatApi *megaChatApi;
    MegaChatRequest *request;
    MegaChatError *error;
    MegaChatListItem *item;
    MegaChatHandle handle;
    MegaChatPresenceConfig *config;
    MegaChatRoom *chat;
    MegaChatMessage *msg;
    MegaChatCall *call;
    bool inProgress;
    int status;
    int width;
    int height;
    char *buffer;
    size_t size;
};

}

#endif // QTMEGACHATEVENT_H
