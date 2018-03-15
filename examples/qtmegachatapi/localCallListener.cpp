#include "localCallListener.h"
#include "callGui.h"
#include <QImage>

LocalCallListener::LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui):
CallListener(megaChatApi,callGui)
{
    frameCounter = 0;
    mMegaChatApi->addChatLocalVideoListener(megaChatVideoListenerDelegate);    
}

LocalCallListener::~LocalCallListener()
{
    mMegaChatApi->removeChatLocalVideoListener(megaChatVideoListenerDelegate);    
}


void LocalCallListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    frameCounter++;
    if((width == 0) || (height == 0) || (frameCounter <= 1))
    {
        return;
    }

    unsigned char *auxBuf = reinterpret_cast<unsigned char*> (buffer);
    QImage *auxImg = new QImage(auxBuf, width, height, QImage::Format_ARGB32);
    this->mCallGui->ui->localRenderer->setStaticImage(auxImg);
    this->mCallGui->ui->localRenderer->enableStaticImage();
}
