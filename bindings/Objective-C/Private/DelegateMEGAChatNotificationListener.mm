
#import "DelegateMEGAChatNotificationListener.h"
#import "MEGAChatMessage+init.h"

using namespace megachat;

DelegateMEGAChatNotificationListener::DelegateMEGAChatNotificationListener(MEGAChatSdk *megaChatSdk, id<MEGAChatNotificationDelegate>listener, bool singleListener) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
}


id<MEGAChatNotificationDelegate>DelegateMEGAChatNotificationListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatNotificationListener::onChatNotification(MegaChatApi* api, MegaChatHandle chatid, MegaChatMessage *msg) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatNotification:chatId:message:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatNotificationDelegate>tempListener = this->listener;
        MegaChatMessage *tempMessage = msg ? msg->copy() : NULL;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onChatNotification:tempMEGAChatSdk chatId:chatid message:tempMessage ? [[MEGAChatMessage alloc] initWithMegaChatMessage:tempMessage cMemoryOwn:YES] : nil];
        });
    }
}
