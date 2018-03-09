#ifndef LOCALCALLLISTENER_H
#define LOCALCALLLISTENER_H
#include "callListener.h"

using namespace megachat;

class LocalCallListener: public CallListener
{   
    public:
        LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui);
        virtual ~ LocalCallListener();
        void onChatCallUpdate(MegaChatApi *api, MegaChatCall *call);
        void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);
};

#endif // LOCALCALLLISTENER_H
