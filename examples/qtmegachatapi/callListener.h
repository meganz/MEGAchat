#ifndef CALLLISTENER_H
#define CALLLISTENER_H
#include "megachatapi.h"
#include "QTMegaChatVideoListener.h"

using namespace megachat;
class CallGui;

class CallListener: public MegaChatVideoListener, public MegaChatCallListener
{
    protected:
        MegaChatApi *mMegaChatApi;
        CallGui *mCallGui;
        QTMegaChatVideoListener *megaChatVideoListenerDelegate;        
    public:
        CallListener(MegaChatApi *megaChatApi, CallGui *callGui);
        virtual ~CallListener();
};

#endif // CALLLISTENER_H

