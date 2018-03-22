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
    char * copyBuff = new char[size];
    memcpy(copyBuff,buffer,size);
    unsigned char* auxBuf = reinterpret_cast<unsigned char*> (copyBuff);
    QImage *auxImg = new QImage(auxBuf, width, height, QImage::Format_RGBA8888);
    this->mCallGui->ui->localRenderer->setStaticImage(auxImg);
    this->mCallGui->ui->localRenderer->enableStaticImage();
}
