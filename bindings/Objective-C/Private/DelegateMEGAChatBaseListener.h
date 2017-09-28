#import "MEGAChatSdk.h"

class DelegateMEGAChatBaseListener {
    
public:
    DelegateMEGAChatBaseListener(MEGAChatSdk *megaChatSDK, bool singleListener = false);
    void setValidListener(bool validListener);
    
protected:
    MEGAChatSdk *megaChatSDK;
    bool singleListener;
    bool validListener;
};
