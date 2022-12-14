#import "DelegateMEGAChatListener.h"
#import "MEGAChatListItem+init.h"
#import "MEGAChatPresenceConfig+init.h"
#import "MEGAChatSdk+init.h"

using namespace megachat;

DelegateMEGAChatListener::DelegateMEGAChatListener(MEGAChatSdk *megaChatSDK, id<MEGAChatDelegate>listener, bool singleListener, ListenerQueueType queueType) {
    this->megaChatSDK = megaChatSDK;
    this->listener = listener;
    this->singleListener = singleListener;
    this->queueType = queueType;
}

id<MEGAChatDelegate>DelegateMEGAChatListener::getUserListener() {
    return listener;
}

void DelegateMEGAChatListener::onChatListItemUpdate(megachat::MegaChatApi *api, megachat::MegaChatListItem *item) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatListItemUpdate:item:)]) {
        MegaChatListItem *tempItem = item->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatListItemUpdate:tempMegaChatSDK item:[[MEGAChatListItem alloc]initWithMegaChatListItem:tempItem cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatListener::onChatInitStateUpdate(megachat::MegaChatApi *api, int newState) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatInitStateUpdate:newState:)]) {
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatInitStateUpdate:tempMegaChatSDK newState:(MEGAChatInit)newState];
        });
    }
}

void DelegateMEGAChatListener::onChatOnlineStatusUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle userHandle, int status, bool inProgress) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatOnlineStatusUpdate:userHandle:status:inProgress:)]) {
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatOnlineStatusUpdate:tempMegaChatSDK userHandle:userHandle status:(MEGAChatStatus)status inProgress:inProgress];
        });
    }
}

void DelegateMEGAChatListener::onChatPresenceConfigUpdate(megachat::MegaChatApi *api, megachat::MegaChatPresenceConfig *config) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatPresenceConfigUpdate:presenceConfig:)]) {
        MegaChatPresenceConfig *tempConfig = config->copy();
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatPresenceConfigUpdate:tempMegaChatSDK presenceConfig:[[MEGAChatPresenceConfig alloc] initWithMegaChatPresenceConfig:tempConfig cMemoryOwn:YES]];
        });
    }
}

void DelegateMEGAChatListener::onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatId, int newState) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatConnectionStateUpdate:chatId:newState:)]) {
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatConnectionStateUpdate:tempMegaChatSDK chatId:chatId newState:newState];
        });
    }
}

void DelegateMEGAChatListener::onChatPresenceLastGreen(megachat::MegaChatApi *api, megachat::MegaChatHandle userHandle, int lastGreen) {
    if (listener != nil && [listener respondsToSelector:@selector(onChatPresenceLastGreen:userHandle:lastGreen:)]) {
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        dispatch(this->queueType, ^{
            [tempListener onChatPresenceLastGreen:tempMegaChatSDK userHandle:userHandle lastGreen:lastGreen];
        });
    }
}

void DelegateMEGAChatListener::onDbError(megachat::MegaChatApi *api, int error, const char *message) {
    if (listener != nil && [listener respondsToSelector:@selector(onDbError:error:message:)]) {
        MEGAChatSdk *tempMegaChatSDK = this->megaChatSDK;
        id<MEGAChatDelegate> tempListener = this->listener;
        NSString *msg = [NSString stringWithUTF8String:message];
        dispatch(this->queueType, ^{
            [tempListener onDbError:tempMegaChatSDK error:(MEGAChatDBError)error message:msg];
        });
    }
}
