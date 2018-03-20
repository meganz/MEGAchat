#include "remoteCallListener.h"
#include "callGui.h"
#include <QImage>

RemoteCallListener::RemoteCallListener(MegaChatApi *megaChatApi, CallGui *callGui):
CallListener(megaChatApi,callGui)
{
    mMegaChatApi->addChatRemoteVideoListener(megaChatVideoListenerDelegate);
}

RemoteCallListener::~RemoteCallListener()
{
    mMegaChatApi->removeChatRemoteVideoListener(megaChatVideoListenerDelegate);
}


void RemoteCallListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    if((width == 0) || (height == 0))
    {
        return;
    }

    unsigned char *auxBuf = reinterpret_cast<unsigned char*> (strdup(buffer));
    oldFrame = actFrame;
    actFrame = new QImage(auxBuf, width, height, QImage::Format_RGBA8888, myImageCleanupHandler, auxBuf);
    this->mCallGui->ui->remoteRenderer->setStaticImage(actFrame);
    this->mCallGui->ui->remoteRenderer->enableStaticImage();
    delete oldFrame;
}
