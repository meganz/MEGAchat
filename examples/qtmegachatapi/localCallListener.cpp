#include "localCallListener.h"
#include "callGui.h"
#include <QImage>

LocalCallListener::LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui):
CallListener(megaChatApi,callGui)
{
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

    unsigned char* auxBuf = reinterpret_cast<unsigned char*> (buffer);
    QImage *Img = new QImage(auxBuf, width, height, QImage::Format_ARGB32);
    this->mCallGui->ui->localRenderer->setStaticImage(Img);
    this->mCallGui->ui->localRenderer->enableStaticImage();
}


void LocalCallListener::onChatCallUpdate(MegaChatApi *api, MegaChatCall *call)
{

}
