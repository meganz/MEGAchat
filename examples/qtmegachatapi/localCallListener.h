#ifndef LOCALCALLLISTENER_H
#define LOCALCALLLISTENER_H
#include "callListener.h"

using namespace megachat;

class LocalCallListener: public CallListener
{
    public:
        LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui, bool hiRes);
        virtual ~ LocalCallListener();
        void onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size);

    protected:
        bool mHiRes;
};

#endif // LOCALCALLLISTENER_H
