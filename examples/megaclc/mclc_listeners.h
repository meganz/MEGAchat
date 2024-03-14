#ifndef MCLC_LISTENERS_H
#define MCLC_LISTENERS_H

/**
 * @file
 * @brief This file defines a set of listeners that are used in different commands of the app. They
 * are not documented as they are very straight forward and is better to see where they are used to
 * get more context about each one.
 */

#include <mega.h>
namespace m = ::mega;
#include <megachatapi.h>
namespace c = ::megachat;

#include <async_utils.h>
#include <string>

namespace mclc::clc_listen
{

class OneShotRequestListener: public m::MegaRequestListener
{
public:
    std::function<void(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e)>
        onRequestFinishFunc;

    explicit OneShotRequestListener(
        std::function<void(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e)> f = {}):
        onRequestFinishFunc(f)
    {}

    void onRequestFinish(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e) override;
};

class OneShotRequestTracker: public m::MegaRequestListener, public megachat::async::ResultHandler
{
public:
    OneShotRequestTracker(m::MegaApi* megaApi):
        mMegaApi(megaApi)
    {}

    ~OneShotRequestTracker();

    void onRequestFinish(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e) override;

    m::MegaRequest* getMegaChatRequestPtr() const
    {
        return mRequest.get();
    }

private:
    std::unique_ptr<m::MegaRequest> mRequest;
    m::MegaApi* mMegaApi;
};

class OneShotTransferListener: public m::MegaTransferListener
{
public:
    std::function<void(m::MegaApi* api, m::MegaTransfer* request, m::MegaError* e)>
        onTransferFinishFunc;

    unsigned lastKnownStage = unsigned(-1);
    bool mLogStage = false;

    explicit OneShotTransferListener(
        std::function<void(m::MegaApi* api, m::MegaTransfer* transfer, m::MegaError* e)> f = {},
        bool ls = false):
        onTransferFinishFunc(f),
        mLogStage(ls)
    {}

    void onTransferFinish(m::MegaApi* api, m::MegaTransfer* request, m::MegaError* e) override;

    bool loggedStart = false;
    void onTransferStart(m::MegaApi*, m::MegaTransfer* request) override;

    void onTransferUpdate(m::MegaApi*, m::MegaTransfer* t) override;
};

class OneShotChatRequestListener: public c::MegaChatRequestListener
{
public:
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest* request)> onRequestStartFunc;
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest* request, c::MegaChatError* e)>
        onRequestFinishFunc;
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest* request)> onRequestUpdateFunc;
    std::function<void(c::MegaChatApi* api, c::MegaChatRequest* request, c::MegaChatError* error)>
        onRequestTemporaryErrorFunc;

    explicit OneShotChatRequestListener(
        std::function<void(c::MegaChatApi* api, c::MegaChatRequest* request, c::MegaChatError* e)>
            f = {}):
        onRequestFinishFunc(f)
    {}

    void onRequestStart(c::MegaChatApi* api, c::MegaChatRequest* request) override;

    void onRequestFinish(c::MegaChatApi* api,
                         c::MegaChatRequest* request,
                         c::MegaChatError* e) override;

    void onRequestUpdate(c::MegaChatApi* api, c::MegaChatRequest* request) override;

    void onRequestTemporaryError(c::MegaChatApi* api,
                                 c::MegaChatRequest* request,
                                 c::MegaChatError* error) override;
};

/**
 * @class CLCChatRequestListener
 * @brief A class to define a global request listener to add to the g_chatApi and define custom
 * checks for the non-interactive mode.
 */
class CLCChatRequestListener: public c::MegaChatRequestListener
{
    void onRequestFinish(c::MegaChatApi* api,
                         c::MegaChatRequest* request,
                         c::MegaChatError* e) override;
};

struct CLCRoomListener: public c::MegaChatRoomListener
{
    c::MegaChatHandle room = c::MEGACHAT_INVALID_HANDLE;

    void onChatRoomUpdate(c::MegaChatApi*, c::MegaChatRoom* chat) override;

    void onMessageLoaded(c::MegaChatApi*, c::MegaChatMessage* msg) override;

    void onMessageReceived(c::MegaChatApi*, c::MegaChatMessage*) override;

    void onMessageUpdate(c::MegaChatApi*, c::MegaChatMessage* msg) override;

    void onHistoryReloaded(c::MegaChatApi*, c::MegaChatRoom* chat) override;

    void onHistoryTruncatedByRetentionTime(c::MegaChatApi*, c::MegaChatMessage* msg) override;
};

struct CLCRoomListenerRecord
{
    bool open = false;
    std::unique_ptr<CLCRoomListener> listener;
    CLCRoomListenerRecord();
};

struct CLCListener: public c::MegaChatListener
{
    void onChatInitStateUpdate(c::MegaChatApi*, int newState) override;

    void onChatConnectionStateUpdate(c::MegaChatApi* api,
                                     c::MegaChatHandle chatid,
                                     int newState) override;
};

struct CLCStateChange
{
    std::atomic<int> state{};
    std::atomic<bool> stateHasChanged{false};

    CLCStateChange() = default;

    CLCStateChange(const CLCStateChange& o):
        state{o.state.load()},
        stateHasChanged{o.stateHasChanged.load()}
    {}

    CLCStateChange(int s, bool sChange):
        state{s},
        stateHasChanged{sChange}
    {}
};

#ifndef KARERE_DISABLE_WEBRTC
class CLCCallListener: public c::MegaChatCallListener
{
    void onChatCallUpdate(megachat::MegaChatApi*, megachat::MegaChatCall* call) override;

    void onChatSessionUpdate(megachat::MegaChatApi*,
                             megachat::MegaChatHandle chatid,
                             megachat::MegaChatHandle callid,
                             megachat::MegaChatSession* session) override;

private:
    void askForParticipantVideo(const megachat::MegaChatHandle chatid,
                                const megachat::MegaChatHandle callid,
                                const megachat::MegaChatSession* session) const;

    bool addParticipantHighResVideo(const megachat::MegaChatHandle chatid,
                                    const megachat::MegaChatHandle callid,
                                    const megachat::MegaChatSession* session) const;

    bool addParticipantLowResVideo(const megachat::MegaChatHandle chatid,
                                   const megachat::MegaChatHandle callid,
                                   const megachat::MegaChatSession* session) const;
};
#endif

struct CLCFinishInfo
{
    c::MegaChatApi* api;
    c::MegaChatRequest* request;
    c::MegaChatError* e;
};

struct CLCChatListener: public c::MegaChatRequestListener
{
private:
    std::mutex m;
    std::map<int, std::function<void(CLCFinishInfo&)>> finishFn;

public:
    void onFinish(int n, std::function<void(CLCFinishInfo&)> f);

    void onRequestStart(c::MegaChatApi*, c::MegaChatRequest*) override {}

    void onRequestFinish(c::MegaChatApi* api,
                         c::MegaChatRequest* request,
                         c::MegaChatError* e) override;

    void onRequestUpdate(c::MegaChatApi*, c::MegaChatRequest*) override {}

    void onRequestTemporaryError(c::MegaChatApi*, c::MegaChatRequest*, c::MegaChatError*) override
    {}
};

class CLCMegaListener: public m::MegaListener
{
public:
    void onRequestStart(m::MegaApi*, m::MegaRequest*) override {}

    void onRequestFinish(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e) override;

    virtual void onRequestUpdate(m::MegaApi*, m::MegaRequest*) override {}

    void onRequestTemporaryError(m::MegaApi*, m::MegaRequest*, m::MegaError*) override {}

    void onTransferStart(m::MegaApi*, m::MegaTransfer*) override {}

    void onTransferFinish(m::MegaApi*, m::MegaTransfer*, m::MegaError*) override {}

    void onTransferUpdate(m::MegaApi*, m::MegaTransfer*) override {}

    void onTransferTemporaryError(m::MegaApi*, m::MegaTransfer*, m::MegaError*) override {}

    void onUsersUpdate(m::MegaApi*, m::MegaUserList* users) override;

    void onNodesUpdate(m::MegaApi*, m::MegaNodeList*) override {}

    void onAccountUpdate(m::MegaApi*) override;

    void onContactRequestsUpdate(m::MegaApi*, m::MegaContactRequestList* requests) override;

    void onReloadNeeded(m::MegaApi* api) override;

#ifdef ENABLE_SYNC
    void onSyncFileStateChanged(m::MegaApi*, m::MegaSync*, std::string*, int) override {}

    void onSyncStateChanged(m::MegaApi*, m::MegaSync*) override {}

    void onGlobalSyncStateChanged(m::MegaApi*) override;
#endif

    void onChatsUpdate(m::MegaApi*, m::MegaTextChatList* chats) override;

    void onEvent(m::MegaApi*, m::MegaEvent* e) override;
};

class CLCMegaGlobalListener: public m::MegaGlobalListener
{
public:
    void onUsersUpdate(m::MegaApi*, m::MegaUserList*) override {}

    void onUserAlertsUpdate(m::MegaApi*, m::MegaUserAlertList*) override {}

    void onNodesUpdate(m::MegaApi*, m::MegaNodeList*) override {}

    void onAccountUpdate(m::MegaApi*) override {}

    void onContactRequestsUpdate(m::MegaApi*, m::MegaContactRequestList*) override {}

    void onReloadNeeded(m::MegaApi*) override {}

#ifdef ENABLE_SYNC
    void onGlobalSyncStateChanged(m::MegaApi*) override {}
#endif

    void onChatsUpdate(m::MegaApi*, m::MegaTextChatList*) override {}

    void onEvent(m::MegaApi*, m::MegaEvent*) override {}
};

class CLCChatRequestTracker: public CLCChatListener, public megachat::async::ResultHandler
{
public:
    CLCChatRequestTracker(megachat::MegaChatApi* megaChatApi):
        mMegaChatApi(megaChatApi)
    {}

    ~CLCChatRequestTracker();

    void onRequestFinish(::megachat::MegaChatApi*,
                         ::megachat::MegaChatRequest* req,
                         ::megachat::MegaChatError* e) override;

    megachat::MegaChatRequest* getMegaChatRequestPtr() const
    {
        return request.get();
    }

private:
    std::unique_ptr<::megachat::MegaChatRequest> request;
    megachat::MegaChatApi* mMegaChatApi;
};

}
#endif // MCLC_LISTENERS_H
