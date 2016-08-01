/**
 * @file megachatapi_impl.h
 * @brief Private header file of the intermediate layer for the MEGA Chat C++ SDK.
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

#ifndef MEGACHATAPI_IMPL_H
#define MEGACHATAPI_IMPL_H


#include "megachatapi.h"
#include <megaapi.h>

#include "../src/chatClient.h"
#include "../src/rtcModule/IRtcModule.h"
#include "../src/rtcModule/IVideoRenderer.h"
#include "../src/rtcModule/IJingleSession.h"

namespace megachat
{

using namespace mega;
using namespace karere;

class MegaChatRequestPrivate : public MegaChatRequest
{

public:
    MegaChatRequestPrivate(int type, MegaChatRequestListener *listener = NULL);
    MegaChatRequestPrivate(MegaChatRequestPrivate &request);
    virtual ~MegaChatRequestPrivate();
    MegaChatRequest *copy();
    virtual int getType() const;
    virtual MegaChatRequestListener *getListener() const;
    virtual const char *getRequestString() const;
    virtual const char* toString() const;
    virtual const char* __str__() const;
    virtual const char* __toString() const;
    virtual long long getNumber() const;

    void setNumber(long long number);
    void setListener(MegaChatRequestListener *listener);

protected:
    int type;
    int tag;
    MegaChatRequestListener *listener;

    long long number;
};

class MegaChatVideoReceiver;
class MegaChatCallPrivate : public MegaChatCall
{
public:
    MegaChatCallPrivate(const char *peer);
    MegaChatCallPrivate(const MegaChatCallPrivate &call);

    virtual ~MegaChatCallPrivate();

    virtual MegaChatCall *copy();

    virtual int getStatus() const;
    virtual int getTag() const;
    virtual MegaHandle getContactHandle() const;

    rtcModule::ICallAnswer *getAnswerObject();

    const char* getPeer() const;
    void setStatus(int status);
    void setTag(int tag);
    void setVideoReceiver(MegaChatVideoReceiver *videoReceiver);
    void setAnswerObject(rtcModule::ICallAnswer *answerObject);

protected:
    int tag;
    int status;
    const char *peer;
    MegaChatVideoReceiver *videoReceiver;
    rtcModule::ICallAnswer *answerObject;
};

class MegaChatVideoFrame
{
public:
    unsigned char *buffer;
    int width;
    int height;
};

class MegaChatVideoReceiver : public rtcModule::IVideoRenderer
{
public:
    MegaChatVideoReceiver(MegaChatApiImpl *chatApi, MegaChatCallPrivate *chatCall, bool local);
    ~MegaChatVideoReceiver();

    void setWidth(int width);
    void setHeight(int height);

    // IVideoRenderer
    virtual unsigned char* getImageBuffer(int width, int height, void** userData);
    virtual void frameComplete(void* userData);
    virtual void onVideoAttach() {}
    virtual void onVideoDetach();
    virtual void clearViewport();
    virtual void released() {}

protected:
    MegaChatApiImpl *chatApi;
    MegaChatCallPrivate *chatCall;
    bool local;
};


class MegaChatApiImpl : public rtcModule::IEventHandler, public IGui, public IGui::IContactList
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, MegaApi *megaApi);
    MegaChatApiImpl(MegaChatApi *chatApi, const char *appKey, const char *appDir);
    virtual ~MegaChatApiImpl();

    void sendPendingRequests();
    void sendPendingEvents();

    void fireOnChatCallStart(MegaChatCallPrivate *call);
    void fireOnChatCallStateChange(MegaChatCallPrivate *call);
    void fireOnChatCallTemporaryError(MegaChatCallPrivate *call, MegaError *error);
    void fireOnChatCallFinish(MegaChatCallPrivate *call, MegaError *error);

    void fireOnChatRemoteVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer, int size);
    void fireOnChatLocalVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer, int size);

    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaError e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaError e);

    MegaChatCallPrivate *getChatCallByPeer(const char* jid);

    MegaChatApi *chatApi;
    MegaApi *megaApi;

//    MegaChatRequestQueue requestQueue;
    std::map<int, MegaChatRequestPrivate *> requestMap;

//    EventQueue eventQueue;
    std::map<int, void*> eventMap;

    karere::Client *mClient;

    std::set<MegaChatListener *> chatListeners;
    std::set<MegaChatVideoListener *> chatLocalVideoListeners;
    std::set<MegaChatVideoListener *> chatRemoteVideoListeners;
    std::map<int, MegaChatCallPrivate *> chatCallMap;
    MegaChatVideoReceiver *localVideoReceiver;


    void setChatStatus(int status, MegaChatRequestListener *listener = NULL);

    // Audio/Video devices
    MegaStringList *getChatAudioInDevices();
    MegaStringList *getChatVideoInDevices();
    bool setChatAudioInDevice(const char *device);
    bool setChatVideoInDevice(const char *device);

    // Calls
    void startChatCall(MegaUser *peer, bool enableVideo = true, MegaChatRequestListener *listener = NULL);
    void answerChatCall(MegaChatCall *call, bool accept, MegaChatRequestListener *listener = NULL);
    void hangAllChatCalls();

    // Listeners
    void addChatListener(MegaChatListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatListener(MegaChatListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);
    void addChatRequestListener(MegaChatRequestListener *listener);
    void removeChatRequestListener(MegaChatRequestListener *listener);

    // rtcModule::IEventHandler implementation
    virtual void onLocalStreamObtained(rtcModule::IVideoRenderer** renderer);
    virtual void onRemoteSdpRecv(rtcModule::IJingleSession* sess, rtcModule::IVideoRenderer** rendererRet);
    virtual void onCallIncomingRequest(rtcModule::ICallAnswer* ctrl);
    virtual void onIncomingCallCanceled(const char *sid, const char *event, const char *by, int accepted, void **userp);
    virtual void onCallEnded(rtcModule::IJingleSession *sess, const char* reason, const char* text, rtcModule::stats::IRtcStats *stats);
    virtual void discoAddFeature(const char *feature);
    virtual void onLocalMediaFail(const char* err, int* cont = nullptr);
    virtual void onCallInit(rtcModule::IJingleSession* sess, int isDataCall);
    virtual void onCallDeclined(const char* fullPeerJid, const char* sid, const char* reason, const char* text, int isDataCall);
    virtual void onCallAnswerTimeout(const char* peer);
    virtual void onCallAnswered(rtcModule::IJingleSession* sess);
    virtual void remotePlayerRemove(rtcModule::IJingleSession* sess, rtcModule::IVideoRenderer* videoRenderer);
    virtual void onMediaRecv(rtcModule::IJingleSession* sess, rtcModule::stats::Options* statOptions);
    virtual void onJingleError(rtcModule::IJingleSession* sess, const char* origin, const char* stanza, const char* origXml, char type);
    virtual void onLocalVideoDisabled();
    virtual void onLocalVideoEnabled();

    // rtcModule::IGui implementation
    virtual ILoginDialog* createLoginDialog();
    virtual IChatWindow* createChatWindow(ChatRoom &room);
    virtual IGui::IContactList& contactList();
    virtual void onIncomingContactRequest();
    virtual rtcModule::IEventHandler* createCallAnswerGui(const std::shared_ptr<rtcModule::ICallAnswer> &ans);
    virtual void show();
    virtual bool visible() const;

    // rtcModule::IContactList implementation
    virtual IContactGui* createContactItem(Contact& contact);
    virtual IContactGui* createGroupChatItem(GroupChatRoom& room);
    virtual void removeContactItem(IContactGui* item);
    virtual void removeGroupChatItem(IContactGui* item);
    virtual IChatWindow& chatWindowForPeer(uint64_t handle);

};


}

#endif // MEGACHATAPI_IMPL_H
