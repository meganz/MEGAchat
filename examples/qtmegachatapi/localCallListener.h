#ifndef LOCALCALLLISTENER_H
#define LOCALCALLLISTENER_H
#include "callListener.h"

using namespace megachat;

class LocalCallListener: public CallListener
{   
    protected:
        int frameCounter;
    public:
        LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui);
        virtual ~ LocalCallListener();
        void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);
};

#endif // LOCALCALLLISTENER_H
