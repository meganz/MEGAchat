#include "localCallListener.h"
#include "callGui.h"
#include <QImage>

LocalCallListener::LocalCallListener(MegaChatApi *megaChatApi, CallGui *callGui, bool hiRes)
    : CallListener(megaChatApi, callGui, MEGACHAT_INVALID_HANDLE, 0)
    , mHiRes(hiRes)
{
    mMegaChatApi->addChatLocalVideoListener(mCallGui->getCall()->getChatid(), mHiRes, megaChatVideoListenerDelegate);
}

LocalCallListener::~LocalCallListener()
{
    mMegaChatApi->removeChatLocalVideoListener(mCallGui->getCall()->getChatid(), mHiRes, megaChatVideoListenerDelegate);
}

void LocalCallListener::onChatVideoData(MegaChatApi *api, MegaChatHandle chatid, int width, int height, char *buffer, size_t size)
{
    QImage *auxImg = CreateFrame(width, height, buffer, size);
    if(auxImg)
    {
        this->mCallGui->ui->videoRenderer->setStaticImage(auxImg);
        this->mCallGui->ui->videoRenderer->enableStaticImage();
    }
}
