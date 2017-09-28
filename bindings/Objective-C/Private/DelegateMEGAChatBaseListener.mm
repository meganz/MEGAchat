#import "DelegateMEGAChatBaseListener.h"

DelegateMEGAChatBaseListener::DelegateMEGAChatBaseListener(MEGAChatSdk *megaChatSDK, bool singleListener) {
    this->megaChatSDK = megaChatSDK;
    this->singleListener = singleListener;
    this->validListener = true;
}

void DelegateMEGAChatBaseListener::setValidListener(bool validListener) {
    this->validListener = validListener;
}
