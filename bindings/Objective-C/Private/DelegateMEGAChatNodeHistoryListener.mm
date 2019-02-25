
#import "DelegateMEGAChatNodeHistoryListener.h"
#import "MEGAChatMessage+init.h"

using namespace megachat;

DelegateMEGAChatNodeHistoryListener::DelegateMEGAChatNodeHistoryListener(MEGAChatSdk *megaChatSdk, id<MEGAChatNodeHistoryDelegate>listener, bool singleListener) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
}

id<MEGAChatNodeHistoryDelegate>DelegateMEGAChatNodeHistoryListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatNodeHistoryListener::onAttachmentLoaded(MegaChatApi *api, MegaChatMessage *msg) {
    if (listener != nil && [listener respondsToSelector:@selector(onAttachmentLoaded:message:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatNodeHistoryDelegate>tempListener = this->listener;
        MegaChatMessage *tempMessage = msg ? msg->copy() : NULL;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onAttachmentLoaded:tempMEGAChatSdk message:tempMessage ? [[MEGAChatMessage alloc] initWithMegaChatMessage:tempMessage cMemoryOwn:YES] : nil];
        });
    }
}

void DelegateMEGAChatNodeHistoryListener::onAttachmentReceived(MegaChatApi *api, MegaChatMessage *msg) {
    if (listener != nil && [listener respondsToSelector:@selector(onAttachmentReceived:message:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatNodeHistoryDelegate>tempListener = this->listener;
        MegaChatMessage *tempMessage = msg->copy();
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onAttachmentReceived:tempMEGAChatSdk message:[[MEGAChatMessage alloc] initWithMegaChatMessage:tempMessage cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatNodeHistoryListener::onAttachmentDeleted(MegaChatApi *api, MegaChatHandle msgid) {
    if (listener != nil && [listener respondsToSelector:@selector(onAttachmentDeleted:messageId:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatNodeHistoryDelegate>tempListener = this->listener;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onAttachmentDeleted:tempMEGAChatSdk messageId:msgid];
        });
    }
}

void DelegateMEGAChatNodeHistoryListener::onTruncate(MegaChatApi *api, MegaChatHandle msgid) {
    if (listener != nil && [listener respondsToSelector:@selector(onAttachmentDeleted:messageId:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatNodeHistoryDelegate>tempListener = this->listener;
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onTruncate:tempMEGAChatSdk messageId:msgid];
        });
    }
    
}
