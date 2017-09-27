#import "DelegateMEGAChatListener.h"
#import "MEGAChatListItem+init.h"
#import "MEGAChatPresenceConfig+init.h"
#import "MEGAChatSdk+init.h"

using namespace megachat;

DelegateMEGAChatListener::DelegateMEGAChatListener(MEGAChatSdk *megaChatSDK, id<MEGAChatDelegate>listener, bool singleListener) {
    this->megaChatSDK = megaChatSDK;
    this->listener = listener;
    this->singleListener = singleListener;
    this->validListener = true;
}

id<MEGAChatDelegate>DelegateMEGAChatListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatListener::setValidListener(bool validListener) {
    this->validListener = validListener;
}

void DelegateMEGAChatListener::onChatListItemUpdate(megachat::MegaChatApi *api, megachat::MegaChatListItem *item) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatListItemUpdate:item:)]) {
        MegaChatListItem *tempItem = item->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatListItemUpdate:this->megaChatSDK item:[[MEGAChatListItem alloc] initWithMegaChatListItem:tempItem cMemoryOwn:YES]];
            }
        });
    }
}

void DelegateMEGAChatListener::onChatInitStateUpdate(megachat::MegaChatApi *api, int newState) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatInitStateUpdate:newState:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatInitStateUpdate:this->megaChatSDK newState:(MEGAChatInit)newState];
            }
        });
    }
}

void DelegateMEGAChatListener::onChatOnlineStatusUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle userHandle, int status, bool inProgress) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatOnlineStatusUpdate:userHandle:status:inProgress:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatOnlineStatusUpdate:this->megaChatSDK userHandle:userHandle status:(MEGAChatStatus)status inProgress:inProgress];
            }
        });
    }
}

void DelegateMEGAChatListener::onChatPresenceConfigUpdate(megachat::MegaChatApi *api, megachat::MegaChatPresenceConfig *config) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatPresenceConfigUpdate:presenceConfig:)]) {
        MegaChatPresenceConfig *tempConfig = config->copy();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (this->validListener) {
                [this->listener onChatPresenceConfigUpdate:this->megaChatSDK presenceConfig:[[MEGAChatPresenceConfig alloc] initWithMegaChatPresenceConfig:tempConfig cMemoryOwn:YES]];
            }
        });
    }
}
