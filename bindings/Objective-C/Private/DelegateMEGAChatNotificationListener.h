
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatNotificationListener : public megachat::MegaChatNotificationListener {
    
public:
    DelegateMEGAChatNotificationListener(MEGAChatSdk *megaChatSdk, id<MEGAChatNotificationDelegate>listener, bool singleListener = true);
    id<MEGAChatNotificationDelegate>getUserListener();
    
    void onChatNotification(megachat::MegaChatApi* api, megachat::MegaChatHandle chatid, megachat::MegaChatMessage *msg);
    
private:
    MEGAChatSdk *megaChatSdk;
    id<MEGAChatNotificationDelegate>listener;
    bool singleListener;
};

