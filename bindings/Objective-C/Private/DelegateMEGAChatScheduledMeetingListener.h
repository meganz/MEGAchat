
#import "megachatapi.h"
#import "MEGAChatSdk.h"
#import "ListenerDispatch.h"

class DelegateMEGAChatScheduledMeetingListener : public megachat::MegaChatScheduledMeetingListener {
    
public:
    DelegateMEGAChatScheduledMeetingListener(MEGAChatSdk *megaChatSdk, id<MEGAChatScheduledMeetingDelegate>listener, bool singleListener = true, ListenerQueueType queueType = ListenerQueueTypeMain);
    id<MEGAChatScheduledMeetingDelegate>getUserListener();
    
    void onChatSchedMeetingUpdate(megachat::MegaChatApi *api, megachat::MegaChatScheduledMeeting *scheduledMeeting);
    void onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, bool append);

private:
    __weak MEGAChatSdk *megaChatSdk;
    __weak id<MEGAChatScheduledMeetingDelegate>listener;
    bool singleListener;
    ListenerQueueType queueType;
};
