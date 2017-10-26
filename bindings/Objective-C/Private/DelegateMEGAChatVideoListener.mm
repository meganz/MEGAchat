
#import "DelegateMEGAChatVideoListener.h"
#import "MEGAChatCall+init.h"

using namespace megachat;

DelegateMEGAChatVideoListener::DelegateMEGAChatVideoListener(MEGAChatSdk *megaChatSdk, id<MEGAChatVideoDelegate>listener, bool singleListener) {
    this->megaChatSdk = megaChatSdk;
    this->listener = listener;
    this->singleListener = singleListener;
}

id<MEGAChatVideoDelegate>DelegateMEGAChatVideoListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatVideoListener::onChatVideoData(megachat::MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatVideoData:chatId:width:height:buffer:)]) {
        MEGAChatSdk *tempMEGAChatSdk = this->megaChatSdk;
        id<MEGAChatVideoDelegate>tempListener = this->listener;
        NSData *data = [[NSData alloc] initWithBytes:buffer length:size];
        dispatch_async(dispatch_get_main_queue(), ^{
            [tempListener onChatVideoData:tempMEGAChatSdk chatId:chatid width:width height:height buffer:data];
        });
    }
}

