#include "localCallListener.h"
#include "callGui.h"
#include <QImage>

LocalCallListener::LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui):
CallListener(megaChatApi,callGui)
{
    oldFrame = NULL;
    actFrame = NULL;
    mMegaChatApi->addChatLocalVideoListener(megaChatVideoListenerDelegate);    
}

LocalCallListener::~LocalCallListener()
{
    mMegaChatApi->removeChatLocalVideoListener(megaChatVideoListenerDelegate);    
}


void LocalCallListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    if((width == 0) || (height == 0))
    {
        return;
    }

    unsigned char *auxBuf = reinterpret_cast<unsigned char*> (strdup(buffer));
    oldFrame = actFrame;
    actFrame = new QImage(auxBuf, width, height, QImage::Format_RGBA8888, myImageCleanupHandler, auxBuf);
    this->mCallGui->ui->localRenderer->disableStaticImage();
    this->mCallGui->ui->localRenderer->setStaticImage(actFrame);
    this->mCallGui->ui->localRenderer->enableStaticImage();
    if(oldFrame)
    {
        delete oldFrame;
    }
}
