#include "remoteCallListener.h"
#include "callGui.h"
#include <QImage>

RemoteCallListener::RemoteCallListener(MegaChatApi *megaChatApi, CallGui *callGui, MegaChatHandle peerid, MegaChatHandle clientid):
CallListener(megaChatApi, callGui, peerid, clientid)
{
    mMegaChatApi->addChatRemoteVideoListener(mCallGui->getCall()->getChatid(), mPeerid, mClientid, megaChatVideoListenerDelegate);
}

RemoteCallListener::~RemoteCallListener()
{
    mMegaChatApi->removeChatRemoteVideoListener(mCallGui->getCall()->getChatid(), mPeerid, mClientid, megaChatVideoListenerDelegate);
}

void RemoteCallListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    QImage *auxImg = CreateFrame(width, height, buffer, size);
    if(auxImg)
    {
        this->mCallGui->ui->videoRenderer->setStaticImage(auxImg);
        this->mCallGui->ui->videoRenderer->enableStaticImage();
    }
}
