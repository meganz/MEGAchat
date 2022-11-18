
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatScheduledMeetingListener : public megachat::MegaChatScheduledMeetingListener {
    
public:
    DelegateMEGAChatScheduledMeetingListener(MEGAChatSdk *megaChatSdk, id<MEGAChatScheduledMeetingDelegate>listener, bool singleListener = true);
    id<MEGAChatScheduledMeetingDelegate>getUserListener();
    
    void onChatSchedMeetingUpdate(megachat::MegaChatApi *api, megachat::MegaChatScheduledMeeting *scheduledMeeting);
    void onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid);

private:
    __weak MEGAChatSdk *megaChatSdk;
    __weak id<MEGAChatScheduledMeetingDelegate>listener;
    bool singleListener;
};
