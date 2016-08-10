/**
 * @file megachatapi.cpp
 * @brief Intermediate layer for the MEGA Chat C++ SDK.
 *
 * (c) 2013-2016 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */


#include <megaapi_impl.h>
#include <megaapi.h>
#include "megachatapi.h"
#include "megachatapi_impl.h"

using namespace mega;
using namespace megachat;

MegaChatCall::~MegaChatCall()
{
}

MegaChatCall *MegaChatCall::copy()
{
    return NULL;
}

int MegaChatCall::getStatus() const
{
    return 0;
}

int MegaChatCall::getTag() const
{
    return 0;
}

MegaHandle MegaChatCall::getContactHandle() const
{
    return INVALID_HANDLE;
}

MegaChatApi::MegaChatApi(mega::MegaApi *megaApi)
{
    this->pImpl = new MegaChatApiImpl(this, megaApi);
}

//MegaChatApi::MegaChatApi(const char *appKey, const char *appDir)
//{
//    this->pImpl = new MegaChatApiImpl(this, appKey, appDir);
//}

void MegaChatApi::setChatStatus(int status, MegaChatRequestListener *listener)
{
    pImpl->setChatStatus(status, listener);
}

MegaStringList *MegaChatApi::getChatAudioInDevices()
{
    return pImpl->getChatAudioInDevices();
}

MegaStringList *MegaChatApi::getChatVideoInDevices()
{
    return pImpl->getChatVideoInDevices();
}

bool MegaChatApi::setChatAudioInDevice(const char *device)
{
    return pImpl->setChatAudioInDevice(device);
}

bool MegaChatApi::setChatVideoInDevice(const char *device)
{
    return pImpl->setChatVideoInDevice(device);
}

void MegaChatApi::startChatCall(MegaUser *peer, bool enableVideo, MegaChatRequestListener *listener)
{
    pImpl->startChatCall(peer, enableVideo, listener);
}

void MegaChatApi::answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener)
{
    pImpl->answerChatCall(call, accept, listener);
}

void MegaChatApi::hangAllChatCalls()
{
    pImpl->hangAllChatCalls();
}

void MegaChatApi::addChatCallListener(MegaChatCallListener *listener)
{
    pImpl->addChatCallListener(listener);
}

void MegaChatApi::removeChatCallListener(MegaChatCallListener *listener)
{
    pImpl->removeChatCallListener(listener);
}

void MegaChatApi::addChatLocalVideoListener(MegaChatVideoListener *listener)
{
    pImpl->addChatLocalVideoListener(listener);
}

void MegaChatApi::removeChatLocalVideoListener(MegaChatVideoListener *listener)
{
    pImpl->removeChatLocalVideoListener(listener);
}

void MegaChatApi::addChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    pImpl->addChatRemoteVideoListener(listener);
}

void MegaChatApi::removeChatRemoteVideoListener(MegaChatVideoListener *listener)
{
    pImpl->removeChatRemoteVideoListener(listener);
}

void MegaChatApi::addChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->addChatRequestListener(listener);
}

void MegaChatApi::removeChatRequestListener(MegaChatRequestListener *listener)
{
    pImpl->removeChatRequestListener(listener);
}

MegaChatRequest::~MegaChatRequest() { }
MegaChatRequest *MegaChatRequest::copy()
{
    return NULL;
}

int MegaChatRequest::getType() const
{
    return 0;
}

const char *MegaChatRequest::getRequestString() const
{
    return NULL;
}

const char *MegaChatRequest::toString() const
{
    return NULL;
}

const char *MegaChatRequest::__str__() const
{
    return NULL;
}

const char *MegaChatRequest::__toString() const
{
    return NULL;
}

int MegaChatRequest::getTag() const
{
    return 0;
}

long long MegaChatRequest::getNumber() const
{
    return 0;
}

int MegaChatRequest::getNumRetry() const
{
    return 0;
}

