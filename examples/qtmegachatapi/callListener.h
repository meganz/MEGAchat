#ifndef CALLLISTENER_H
#define CALLLISTENER_H
#include "megachatapi.h"
#include "QTMegaChatVideoListener.h"
#include <QImage>

using namespace megachat;
class CallGui;

class CallListener: public MegaChatVideoListener
{
    protected:
        MegaChatApi *mMegaChatApi;
        CallGui *mCallGui;
        QTMegaChatVideoListener *megaChatVideoListenerDelegate;
        MegaChatHandle mPeerid;
    public:
        CallListener(MegaChatApi *megaChatApi, CallGui *callGui, MegaChatHandle peerid);
        QImage * CreateFrame(int width, int height, char *buffer, size_t size);
        static void myImageCleanupHandler(void *info);
        virtual ~CallListener();
};

#endif // CALLLISTENER_H

