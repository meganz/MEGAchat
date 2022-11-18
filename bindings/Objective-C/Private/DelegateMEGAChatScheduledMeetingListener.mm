
#import "DelegateMEGAChatScheduledMeetingListener.h"
#import "MEGAChatScheduledMeeting+init.h"

DelegateMEGAChatScheduledMeetingListener::DelegateMEGAChatScheduledMeetingListener(MEGAChatSdk *megaChatSdk, id<MEGAChatScheduledMeetingDelegate>listener, bool singleListener) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
}


id<MEGAChatScheduledMeetingDelegate>DelegateMEGAChatScheduledMeetingListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatScheduledMeetingListener::onChatSchedMeetingUpdate(megachat::MegaChatApi *api, megachat::MegaChatScheduledMeeting *scheduledMeeting) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatSchedMeetingUpdate:scheduledMeeting:)]) {
        megachat::MegaChatScheduledMeeting *tempScheduledMeeting = scheduledMeeting->copy();
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatScheduledMeetingDelegate>tempListener = this->listener;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onChatSchedMeetingUpdate:tempMEGAChatSdk scheduledMeeting:[MEGAChatScheduledMeeting.alloc initWithMegaChatScheduledMeeting:tempScheduledMeeting cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatScheduledMeetingListener::onSchedMeetingOccurrencesUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid) {
    if (listener != nil && [listener respondsToSelector:@selector(onSchedMeetingOccurrencesUpdate:chatId:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatScheduledMeetingDelegate>tempListener = this->listener;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onSchedMeetingOccurrencesUpdate:tempMEGAChatSdk chatId:chatid];
        });
    }
}
