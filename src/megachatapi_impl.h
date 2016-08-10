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

//the megaapi.h header needs this defined externally
//#ifndef ENABLE_CHAT
//    #define ENABLE_CHAT 1
//#endif
#include <megaapi.h>
#include <megaapi_impl.h>

#include <IRtcModule.h>
#include <IVideoRenderer.h>
#include <IJingleSession.h>
#include <chatClient.h>
#include <chatd.h>
#include <sdkApi.h>
#include <mstrophepp.h>

namespace megachat
{

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
    virtual int getTag() const;
    virtual long long getNumber() const;
    virtual int getNumRetry() const;

    void setTag(int tag);
    void setListener(MegaChatRequestListener *listener);
    void setNumber(long long number);
    void setNumRetry(int retry);

protected:
    int type;
    int tag;
    MegaChatRequestListener *listener;

    long long number;
    int retry;
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
    MegaChatVideoReceiver(MegaChatApiImpl *chatApi, MegaChatCallPrivate *call, bool local);
    ~MegaChatVideoReceiver();

    void setWidth(int width);
    void setHeight(int height);

    // rtcModule::IVideoRenderer implementation
    virtual unsigned char* getImageBuffer(unsigned short width, unsigned short height, void** userData);
    virtual void frameComplete(void* userData);
    virtual void onVideoAttach();
    virtual void onVideoDetach();
    virtual void clearViewport();
    virtual void released();

protected:
    MegaChatApiImpl *chatApi;
    MegaChatCallPrivate *call;
    bool local;
};

//Thread safe request queue
class ChatRequestQueue
{
    protected:
        std::deque<MegaChatRequestPrivate *> requests;
        MegaMutex mutex;

    public:
        ChatRequestQueue();
        void push(MegaChatRequestPrivate *request);
        void push_front(MegaChatRequestPrivate *request);
        MegaChatRequestPrivate * pop();
        void removeListener(MegaChatRequestListener *listener);
};

//Thread safe transfer queue
class EventQueue
{
protected:
    std::deque<void *> events;
    MegaMutex mutex;

public:
    EventQueue();
    void push(void* event);
    void push_front(void *event);
    void* pop();
};


class MegaChatApiImpl :
        public karere::IGui,
        public karere::IGui::ILoginDialog,
        public karere::IGui::IChatWindow //: public rtcModule::IEventHandler, public IGui, public IGui::IContactList
{
public:

    MegaChatApiImpl(MegaChatApi *chatApi, MegaApi *megaApi);
    MegaChatApiImpl(MegaChatApi *chatApi, const char *appKey, const char *appDir);
    virtual ~MegaChatApiImpl();

private:
    MegaChatApi *chatApi;
    MegaApi *megaApi;

    karere::Client *mClient;
    chatd::Chat *mChat;

    MegaWaiter *waiter;
    MegaThread thread;
    int threadExit;
    static void *threadEntryPoint(void *param);
    void loop();

    void init(MegaChatApi *chatApi, MegaApi *megaApi);

    ChatRequestQueue requestQueue;
    EventQueue eventQueue;

    std::set<MegaChatRequestListener *> requestListeners;
    std::set<MegaChatCallListener *> callListeners;
    std::set<MegaChatVideoListener *> localVideoListeners;
    std::set<MegaChatVideoListener *> remoteVideoListeners;

    int reqtag;
    std::map<int, MegaChatRequestPrivate *> requestMap;
    std::map<int, MegaChatCallPrivate *> callMap;
    MegaChatVideoReceiver *localVideoReceiver;


public:    
    static void megaApiPostMessage(void* msg);
    void postMessage(void *msg);

    void sendPendingRequests();
    void sendPendingEvents();

    void fireOnChatRequestStart(MegaChatRequestPrivate *request);
    void fireOnChatRequestFinish(MegaChatRequestPrivate *request, MegaError e);
    void fireOnChatRequestUpdate(MegaChatRequestPrivate *request);
    void fireOnChatRequestTemporaryError(MegaChatRequestPrivate *request, MegaError e);

    void fireOnChatCallStart(MegaChatCallPrivate *call);
    void fireOnChatCallStateChange(MegaChatCallPrivate *call);
    void fireOnChatCallTemporaryError(MegaChatCallPrivate *call, MegaError *e);
    void fireOnChatCallFinish(MegaChatCallPrivate *call, MegaError *e);

    void fireOnChatRemoteVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer);
    void fireOnChatLocalVideoData(MegaChatCallPrivate *call, int width, int height, char*buffer);

    MegaChatCallPrivate *getChatCallByPeer(const char* jid);

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
    void addChatCallListener(MegaChatCallListener *listener);
    void addChatRequestListener(MegaChatRequestListener *listener);
    void addChatLocalVideoListener(MegaChatVideoListener *listener);
    void addChatRemoteVideoListener(MegaChatVideoListener *listener);
    void removeChatCallListener(MegaChatCallListener *listener);
    void removeChatRequestListener(MegaChatRequestListener *listener);
    void removeChatLocalVideoListener(MegaChatVideoListener *listener);
    void removeChatRemoteVideoListener(MegaChatVideoListener *listener);

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

    // karere::IGui implementation
    virtual karere::IGui::ILoginDialog *createLoginDialog();
    virtual karere::IGui::IChatWindow* createChatWindow(karere::ChatRoom &room);
    virtual karere::IGui::IContactList& contactList();
    virtual void onIncomingContactRequest(const MegaContactRequest& req);
    virtual rtcModule::IEventHandler* createCallAnswerGui(const std::shared_ptr<rtcModule::ICallAnswer> &ans);
    virtual void show();
    virtual bool visible() const;

    // karere::ILoginDialog
    virtual promise::Promise<std::pair<std::string, std::string>> requestCredentials();

//    // rtcModule::IContactList implementation
//    virtual IContactGui* createContactItem(Contact& contact);
//    virtual IContactGui* createGroupChatItem(GroupChatRoom& room);
//    virtual void removeContactItem(IContactGui* item);
//    virtual void removeGroupChatItem(IContactGui* item);
//    virtual IChatWindow& chatWindowForPeer(uint64_t handle);

};


}

#endif // MEGACHATAPI_IMPL_H
