#include "callListener.h"

CallListener::CallListener(MegaChatApi *megaChatApi, CallGui *callGui)
 : MegaChatVideoListener()
{    
    mMegaChatApi = megaChatApi;
    mCallGui = callGui;
    megaChatVideoListenerDelegate = new QTMegaChatVideoListener(mMegaChatApi, this);    
}
 CallListener::~ CallListener()
 {
     delete megaChatVideoListenerDelegate;     
 }
