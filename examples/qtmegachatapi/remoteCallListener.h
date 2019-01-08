#ifndef REMOTECALLLISTENER_H
#define REMOTECALLLISTENER_H
#include "callListener.h"

using namespace megachat;

class RemoteCallListener: public CallListener
{
    public:
        RemoteCallListener(MegaChatApi *megaChatApi, CallGui *callGui, MegaChatHandle peerid);
        virtual ~RemoteCallListener();
        void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);
};

#endif // REMOTECALLLISTENER_H
