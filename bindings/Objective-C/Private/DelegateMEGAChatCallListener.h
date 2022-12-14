
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatCallListener : public megachat::MegaChatCallListener {
    
public:
    DelegateMEGAChatCallListener(MEGAChatSdk *megaChatSdk, id<MEGAChatCallDelegate>listener, bool singleListener = true);
    id<MEGAChatCallDelegate>getUserListener();
    
    void onChatCallUpdate(megachat::MegaChatApi* api, megachat::MegaChatCall *call);
    void onChatSessionUpdate(megachat::MegaChatApi *api, megachat::MegaChatHandle chatid, megachat::MegaChatHandle callid, megachat::MegaChatSession *session);
    
private:
    __weak MEGAChatSdk *megaChatSdk;
    __weak id<MEGAChatCallDelegate>listener;
    bool singleListener;
};
