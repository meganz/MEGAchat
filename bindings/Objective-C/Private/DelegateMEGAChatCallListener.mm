
#import "DelegateMEGAChatCallListener.h"
#import "MEGAChatCall+init.h"
#import "MEGAChatError+init.h"
#import "MEGAChatSession+init.h"

using namespace megachat;

DelegateMEGAChatCallListener::DelegateMEGAChatCallListener(MEGAChatSdk *megaChatSdk, id<MEGAChatCallDelegate>listener, bool singleListener, ListenerQueueType queueType) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
    this->queueType = queueType;
}

id<MEGAChatCallDelegate>DelegateMEGAChatCallListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatCallListener::onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatCallUpdate:call:)]) {
        MegaChatCall *tempCall = call->copy();
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatCallDelegate>tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatCallUpdate:tempMEGAChatSdk call:[[MEGAChatCall alloc] initWithMegaChatCall:tempCall cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatCallListener::onChatSessionUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, megachat::MegaChatHandle callid, megachat::MegaChatSession *session) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatSessionUpdate:chatId:callId:session:)]) {
        MegaChatSession *tempSession = session->copy();
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatCallDelegate>tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatSessionUpdate:tempMEGAChatSdk chatId:chatid callId:callid session:[MEGAChatSession.alloc initWithMegaChatSession:tempSession cMemoryOwn:YES]];
        });
    }
}
