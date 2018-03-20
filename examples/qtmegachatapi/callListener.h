#ifndef CALLLISTENER_H
#define CALLLISTENER_H
#include "megachatapi.h"
#include "QTMegaChatVideoListener.h"

using namespace megachat;
class CallGui;

class CallListener: public MegaChatVideoListener
{
    protected:
        QImage * actFrame;
        QImage * oldFrame;
        MegaChatApi *mMegaChatApi;
        CallGui *mCallGui;
        QTMegaChatVideoListener *megaChatVideoListenerDelegate;        
    public:
        CallListener(MegaChatApi *megaChatApi, CallGui *callGui);
        static void myImageCleanupHandler(void *info);
        virtual ~CallListener();
};

#endif // CALLLISTENER_H

