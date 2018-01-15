#import "MEGAChatRoomDelegate.h"
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatRoomListener : public megachat::MegaChatRoomListener {
    
public:
    
    DelegateMEGAChatRoomListener(MEGAChatSdk *megaChatSDK, id<MEGAChatRoomDelegate>listener, bool singleListener = true);
    id<MEGAChatRoomDelegate>getUserListener();
    
    void onChatRoomUpdate(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
    void onMessageLoaded(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onMessageReceived(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onMessageUpdate(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onHistoryReloaded(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
    
private:
    MEGAChatSdk *megaChatSDK;
    id<MEGAChatRoomDelegate>listener;
    bool singleListener;
};
