
#import "DelegateMEGAChatCallListener.h"
#import "MEGAChatCall+init.h"
#import "MEGAChatError+init.h"

using namespace megachat;

DelegateMEGAChatCallListener::DelegateMEGAChatCallListener(MEGAChatSdk *megaChatSdk, id<MEGAChatCallDelegate>listener, bool singleListener) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
}

id<MEGAChatCallDelegate>DelegateMEGAChatCallListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatCallListener::onChatCallUpdate(megachat::MegaChatApi *api, megachat::MegaChatCall *call) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatCallUpdate:call:)]) {
        MegaChatCall *tempCall = call->copy();
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatCallDelegate>tempListener = this->listener;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onChatCallUpdate:tempMEGAChatSdk call:[[MEGAChatCall alloc] initWithMegaChatCall:tempCall cMemoryOwn:YES]];
        });
    }
}
