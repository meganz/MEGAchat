
#import "megachatapi.h"
#import "MEGAChatSdk.h"
#import "ListenerDispatch.h"

class DelegateMEGAChatListener : public megachat::MegaChatListener {
    
public:
    
    DelegateMEGAChatListener(MEGAChatSdk *megaChatSDK, id<MEGAChatDelegate>listener, bool singleListener = true, ListenerQueueType queueType = ListenerQueueTypeMain);
    id<MEGAChatDelegate>getUserListener();
    
    void onChatListItemUpdate(megachat::MegaChatApi *api, megachat::MegaChatListItem *item);
    void onChatInitStateUpdate(megachat::MegaChatApi *api, int newState);
    void onChatOnlineStatusUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle userHandle, int status, bool inProgress);
    void onChatPresenceConfigUpdate(megachat::MegaChatApi *api, megachat::MegaChatPresenceConfig *config);
    void onChatConnectionStateUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatId, int newState);
    void onChatPresenceLastGreen(megachat::MegaChatApi* api, megachat::MegaChatHandle userHandle, int lastGreen);
    void onDbError(megachat::MegaChatApi* api, int error, const char* message);
    
private:
    __weak MEGAChatSdk *megaChatSDK;
    __weak id<MEGAChatDelegate>listener;
    bool singleListener;
    ListenerQueueType queueType;
};
