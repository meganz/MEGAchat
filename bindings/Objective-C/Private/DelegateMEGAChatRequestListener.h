
#import "megachatapi.h"
#import "MEGAChatSdk.h"
#import "ListenerDispatch.h"

class DelegateMEGAChatRequestListener : public megachat::MegaChatRequestListener {

public:
    
    DelegateMEGAChatRequestListener(MEGAChatSdk *megaChatSDK, id<MEGAChatRequestDelegate>listener, bool singleListener = true, ListenerQueueType queueType = ListenerQueueTypeMain);
    id<MEGAChatRequestDelegate>getUserListener();
    
    void onRequestStart(megachat::MegaChatApi *api, megachat::MegaChatRequest *request);
    void onRequestFinish(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError *e);
    void onRequestUpdate(megachat::MegaChatApi *api, megachat::MegaChatRequest *request);
    void onRequestTemporaryError(megachat::MegaChatApi *api, megachat::MegaChatRequest *request, megachat::MegaChatError *e);
    
private:
    __weak MEGAChatSdk *megaChatSDK;
    __weak id<MEGAChatRequestDelegate>listener;
    bool singleListener;
    ListenerQueueType queueType;
};
