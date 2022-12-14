
#import "megachatapi.h"
#import "MEGAChatSdk.h"
#import "ListenerDispatch.h"

class DelegateMEGAChatRoomListener : public megachat::MegaChatRoomListener {
    
public:
    
    DelegateMEGAChatRoomListener(MEGAChatSdk *megaChatSDK,
                                 id<MEGAChatRoomDelegate>listener,
                                 bool singleListener = true,
                                 ListenerQueueType queueType = ListenerQueueTypeMain);
    id<MEGAChatRoomDelegate>getUserListener();
    
    void onChatRoomUpdate(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
    void onMessageLoaded(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onMessageReceived(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onMessageUpdate(megachat::MegaChatApi *api, megachat::MegaChatMessage *message);
    void onHistoryReloaded(megachat::MegaChatApi *api, megachat::MegaChatRoom *chat);
    void onReactionUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle msgid, const char *reaction, int count);
    
private:
    __weak MEGAChatSdk *megaChatSDK;
    __weak id<MEGAChatRoomDelegate>listener;
    bool singleListener;
    ListenerQueueType queueType;
};
