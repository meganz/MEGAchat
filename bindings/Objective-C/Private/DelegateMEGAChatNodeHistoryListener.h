
#import "megachatapi.h"
#import "MEGAChatSdk.h"

class DelegateMEGAChatNodeHistoryListener : public megachat::MegaChatNodeHistoryListener {
    
public:
    DelegateMEGAChatNodeHistoryListener(MEGAChatSdk *megaChatSdk, id<MEGAChatNodeHistoryDelegate>listener, bool singleListener = true);
    id<MEGAChatNodeHistoryDelegate>getUserListener();
    
    void onAttachmentLoaded(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg);
    void onAttachmentReceived(megachat::MegaChatApi *api, megachat::MegaChatMessage *msg);
    void onAttachmentDeleted(megachat::MegaChatApi *api, megachat::MegaChatHandle msgid);
    void onTruncate(megachat::MegaChatApi *api, megachat::MegaChatHandle msgid);
    
private:
    __weak MEGAChatSdk *megaChatSdk;
    id<MEGAChatNodeHistoryDelegate>listener;
    bool singleListener;
};
