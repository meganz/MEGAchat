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


    char * copyBuff = new char[size];
    memcpy(copyBuff,buffer,size);
    unsigned char* auxBuf = reinterpret_cast<unsigned char*> (copyBuff);
    QImage *Img = new QImage(auxBuf, width, height, QImage::Format_RGBA8888);
    this->mCallGui->ui->remoteRenderer->setStaticImage(Img);
    this->mCallGui->ui->remoteRenderer->enableStaticImage();
}
