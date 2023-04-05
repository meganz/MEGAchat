
#import "DelegateMEGAChatScheduledMeetingListener.h"
#import "MEGAChatScheduledMeeting+init.h"

using namespace megachat;

DelegateMEGAChatScheduledMeetingListener::DelegateMEGAChatScheduledMeetingListener(MEGAChatSdk *megaChatSdk, id<MEGAChatScheduledMeetingDelegate>listener, bool singleListener, ListenerQueueType queueType) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
    this->queueType = queueType;
}

id<MEGAChatScheduledMeetingDelegate>DelegateMEGAChatScheduledMeetingListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatScheduledMeetingListener::onChatSchedMeetingUpdate(MegaChatApi *api, MegaChatScheduledMeeting *scheduledMeeting) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatSchedMeetingUpdate:scheduledMeeting:)]) {
        MegaChatScheduledMeeting *tempScheduledMeeting = scheduledMeeting->copy();
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatScheduledMeetingDelegate>tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatSchedMeetingUpdate:tempMEGAChatSdk scheduledMeeting:[MEGAChatScheduledMeeting.alloc initWithMegaChatScheduledMeeting:tempScheduledMeeting cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatScheduledMeetingListener::onSchedMeetingOccurrencesUpdate(MegaChatApi *api, MegaChatHandle chatid, bool append) {
    if (listener != nil && [listener respondsToSelector:@selector(onSchedMeetingOccurrencesUpdate:chatId:append:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatScheduledMeetingDelegate>tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onSchedMeetingOccurrencesUpdate:tempMEGAChatSdk chatId:chatid append:append];
        });
    }
}
