#import "DelegateMEGAChatRequestListener.h"
#import "MEGAChatRequest+init.h"
#import "MEGAChatError+init.h"
#import "MEGAChatSdk+init.h"

using namespace megachat;

DelegateMEGAChatRequestListener::DelegateMEGAChatRequestListener(MEGAChatSdk *megaChatSDK, id<MEGAChatRequestDelegate>listener, bool singleListener) {
    this->megaChatSDK = megaChatSDK;
    this->listener = listener;
    this->singleListener = singleListener;
    this->validListener = true;
}

id<MEGAChatRequestDelegate>DelegateMEGAChatRequestListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatRequestListener::setValidListener(bool validListener) {
    this->validListener = validListener;
}

void DelegateMEGAChatRequestListener::onRequestStart(MegaChatApi *api, MegaChatRequest *request) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestStart:request:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatRequestDelegate> tempListener = this->listener;
        bool tempValidListener = validListener;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (tempValidListener) {
                [tempListener onChatRequestStart:tempMegaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES]];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestFinish:request:error:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MegaChatError *tempError = e->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatRequestDelegate> tempListener = this->listener;
        bool tempSingleListener = this->singleListener;
        bool tempValidListener = validListener;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (tempValidListener) {
                [tempListener onChatRequestFinish:tempMegaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES] error:[[MEGAChatError alloc] initWithMegaChatError:tempError cMemoryOwn:YES]];
            }
            if (tempSingleListener) {
                [megaChatSDK freeRequestListener:this];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestUpdate(MegaChatApi *api, MegaChatRequest *request) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestUpdate:request:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatRequestDelegate> tempListener = this->listener;
        bool tempValidListener = validListener;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (tempValidListener) {
                [tempListener onChatRequestUpdate:tempMegaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES]];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestTemporaryError:request:error:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MegaChatError *tempError = e->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatRequestDelegate> tempListener = this->listener;
        bool tempValidListener = validListener;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (tempValidListener) {
                [tempListener onChatRequestTemporaryError:tempMegaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES] error:[[MEGAChatError alloc] initWithMegaChatError:tempError cMemoryOwn:YES]];
            }
        });
    }
}
