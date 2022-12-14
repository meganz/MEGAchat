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
        OnChatPresenceLastGreen,
        OnChatRoomUpdate,
        OnMessageLoaded,
        OnMessageReceived,
        OnMessageUpdate,
        OnHistoryReloaded,
        OnChatNotification,
        OnChatVideoData,
        OnChatCallUpdate,
        onChatSessionUpdate,
        OnAttachmentLoaded,
        OnAttachmentReceived,
        OnAttachmentDeleted,
        OnAttachmentTruncated,
        OnReactionUpdated,
        OnHistoryTruncatedByRetentionTime,
        OnDbError,
        onChatSchedMeetingUpdate,
        onSchedMeetingOccurrencesChange,
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
    MegaChatSession *getChatSession();
    MegaChatScheduledMeeting* getSchedMeeting();
    MegaChatScheduledMeetingList* getSchedMeetingOccurr();
    MegaChatHandle getChatCallid();

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
    void setChatSession(MegaChatSession *session);
    void setSchedMeeting(MegaChatScheduledMeeting* sm);
    void setSchedMeetingOccurr(MegaChatScheduledMeetingList* l);
    void setChatCallid(MegaChatHandle callid);

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
    MegaChatSession *session;
    MegaChatHandle callid;
    MegaChatScheduledMeeting* schedMeeting;
    MegaChatScheduledMeetingList* schedMeetingOccurr;
    bool inProgress;
    int status;
    int width;
    int height;
    char *buffer;
    size_t size;
};

}

#endif // QTMEGACHATEVENT_H
