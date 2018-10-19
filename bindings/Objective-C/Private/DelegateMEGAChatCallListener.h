
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatCallListener : public megachat::MegaChatCallListener {
    
public:
    DelegateMEGAChatCallListener(MEGAChatSdk *megaChatSdk, id<MEGAChatCallDelegate>listener, bool singleListener = true);
    id<MEGAChatCallDelegate>getUserListener();
    
    void onChatCallUpdate(megachat::MegaChatApi* api, megachat::MegaChatCall *call);
    
private:
    MEGAChatSdk *megaChatSdk;
    id<MEGAChatCallDelegate>listener;
    bool singleListener;
};
