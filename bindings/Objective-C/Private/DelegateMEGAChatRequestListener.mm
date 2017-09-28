#import "DelegateMEGAChatRequestListener.h"
#import "MEGAChatRequest+init.h"
#import "MEGAChatError+init.h"
#import "MEGAChatSdk+init.h"

using namespace megachat;

DelegateMEGAChatRequestListener::DelegateMEGAChatRequestListener(MEGAChatSdk *megaChatSDK, id<MEGAChatRequestDelegate>listener, bool singleListener) : DelegateMEGAChatBaseListener::DelegateMEGAChatBaseListener(megaChatSDK, singleListener) {
    this->listener = listener;
}

id<MEGAChatRequestDelegate>DelegateMEGAChatRequestListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatRequestListener::onRequestStart(MegaChatApi *api, MegaChatRequest *request) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestStart:request:)]) {
        MegaChatRequest *tempRequest = request->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatRequestStart:this->megaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES]];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestFinish:request:error:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MegaChatError *tempError = e->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatRequestFinish:this->megaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES] error:[[MEGAChatError alloc] initWithMegaChatError:tempError cMemoryOwn:YES]];
            }
            if (this->singleListener) {
                [megaChatSDK freeRequestListener:this];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestUpdate(MegaChatApi *api, MegaChatRequest *request) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestUpdate:request:)]) {
        MegaChatRequest *tempRequest = request->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatRequestUpdate:this->megaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES]];
            }
        });
    }
}

void DelegateMEGAChatRequestListener::onRequestTemporaryError(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatRequestTemporaryError:request:error:)]) {
        MegaChatRequest *tempRequest = request->copy();
        MegaChatError *tempError = e->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatRequestTemporaryError:this->megaChatSDK request:[[MEGAChatRequest alloc] initWithMegaChatRequest:tempRequest cMemoryOwn:YES] error:[[MEGAChatError alloc] initWithMegaChatError:tempError cMemoryOwn:YES]];
            }
        });
    }
}
