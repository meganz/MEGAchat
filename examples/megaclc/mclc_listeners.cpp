#include "mclc_listeners.h"

#include "mclc_general_utils.h"
#include "mclc_globals.h"
#include "mclc_logging.h"
#include "mclc_reports.h"

#include <karereId.h>

namespace mclc::clc_listen
{

void OneShotRequestListener::onRequestFinish(m::MegaApi* api,
                                             m::MegaRequest* request,
                                             m::MegaError* e)
{
    if (onRequestFinishFunc)
        onRequestFinishFunc(api, request, e);
    delete this; // one-shot is done so auto-delete
}

OneShotRequestTracker::~OneShotRequestTracker()
{
    if (!resultReceived)
    {
        mMegaApi->removeRequestListener(this);
    }
}

void OneShotRequestTracker::onRequestFinish(m::MegaApi*, m::MegaRequest* request, m::MegaError* e)
{
    mRequest.reset(request ? request->copy() : nullptr);
    finish(e->getErrorCode(), e->getErrorString() ? e->getErrorString() : "");
}

void OneShotTransferListener::onTransferFinish(m::MegaApi* api,
                                               m::MegaTransfer* request,
                                               m::MegaError* e)
{
    if (onTransferFinishFunc)
        onTransferFinishFunc(api, request, e);
    delete this; // one-shot is done so auto-delete
}

void OneShotTransferListener::onTransferStart(m::MegaApi*, m::MegaTransfer* request)
{
    if (!loggedStart)
    {
        loggedStart = true;
        std::string path;
        if (request->getPath())
            path = request->getPath();
        if (request->getParentPath())
            path = request->getParentPath();
        clc_console::conlock(std::cout)
            << "transfer starts, tag: " << request->getTag() << ": " << path << std::endl;
    }
}

void OneShotTransferListener::onTransferUpdate(m::MegaApi*, m::MegaTransfer* t)
{
    if (mLogStage && lastKnownStage != t->getStage())
    {
        lastKnownStage = t->getStage();
        if (lastKnownStage >= m::MegaTransfer::STAGE_SCAN &&
            lastKnownStage <= m::MegaTransfer::STAGE_TRANSFERRING_FILES)
        {
            clc_console::conlock(std::cout)
                << "Transfer stage: " << t->stageToString(t->getStage()) << std::endl;
        }
    }
}

void OneShotChatRequestListener::onRequestStart(c::MegaChatApi* api, c::MegaChatRequest* request)
{
    if (onRequestStartFunc)
        onRequestStartFunc(api, request);
}

void OneShotChatRequestListener::onRequestFinish(c::MegaChatApi* api,
                                                 c::MegaChatRequest* request,
                                                 c::MegaChatError* e)
{
    if (onRequestFinishFunc)
        onRequestFinishFunc(api, request, e);
    delete this; // one-shot is done so auto-delete
}

void OneShotChatRequestListener::onRequestUpdate(c::MegaChatApi* api, c::MegaChatRequest* request)
{
    if (onRequestUpdateFunc)
        onRequestUpdateFunc(api, request);
}

void OneShotChatRequestListener::onRequestTemporaryError(c::MegaChatApi* api,
                                                         c::MegaChatRequest* request,
                                                         c::MegaChatError* error)
{
    if (onRequestTemporaryErrorFunc)
        onRequestTemporaryErrorFunc(api, request, error);
}

void CLCChatRequestListener::onRequestFinish(c::MegaChatApi*,
                                             c::MegaChatRequest* request,
                                             c::MegaChatError* e)
{
    assert(request && e);
    if ((request->getType() == c::MegaChatRequest::TYPE_LOGOUT) &&
        (clc_log::check_err("Chat Logout", e)))
    {
        clc_global::g_chatFinishedLogout = true;
    }
#ifndef KARERE_DISABLE_WEBRTC
    else if ((request->getType() == c::MegaChatRequest::TYPE_REQUEST_LOW_RES_VIDEO) &&
             (e->getErrorCode() != c::MegaChatError::ERROR_OK))
    {
        auto ch = request->getChatHandle();
        std::unique_ptr<c::MegaChatCall> call(clc_global::g_chatApi->getChatCall(ch));
        if (!call)
        {
            return;
        }
        auto handleList = request->getMegaHandleList();
        if (!handleList)
        {
            return;
        }
        auto callid = call->getHandle();
        for (unsigned i = 0; i < handleList->size(); ++i)
        {
            auto clientId = handleList->get(i);
            clc_global::g_callVideoParticipants.removeParticipant(callid, clientId);
        }
    }
    else if ((request->getType() == c::MegaChatRequest::TYPE_REQUEST_HIGH_RES_VIDEO) &&
             (e->getErrorCode() != c::MegaChatError::ERROR_OK))
    {
        auto ch = request->getChatHandle();
        std::unique_ptr<c::MegaChatCall> call(clc_global::g_chatApi->getChatCall(ch));
        if (!call)
        {
            return;
        }
        auto clientId = request->getUserHandle();
        auto callid = call->getHandle();
        clc_global::g_callVideoParticipants.removeParticipant(callid, clientId);
    }
#endif
}

CLCRoomListenerRecord::CLCRoomListenerRecord():
    listener(new CLCRoomListener)
{}

void CLCListener::onChatInitStateUpdate(c::MegaChatApi*, int newState)
{
    std::string message = "Status update : ";
    switch (newState)
    {
        case c::MegaChatApi::INIT_ERROR:
        {
            message += "INIT_ERROR";
            clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_ERROR,
                            message,
                            clc_log::ELogWriter::MEGA_CHAT);
            break;
        }
        case c::MegaChatApi::INIT_WAITING_NEW_SESSION:
        {
            message += "INIT_WAITING_NEW_SESSION";
            clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_INFO,
                            message,
                            clc_log::ELogWriter::MEGA_CHAT);
            break;
        }
        case c::MegaChatApi::INIT_OFFLINE_SESSION:
        {
            message += "INIT_OFFLINE_SESSION";
            clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_INFO,
                            message,
                            clc_log::ELogWriter::MEGA_CHAT);
            break;
        }
        case c::MegaChatApi::INIT_ONLINE_SESSION:
        {
            message += "INIT_ONLINE_SESSION";
            clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_INFO,
                            message,
                            clc_log::ELogWriter::MEGA_CHAT);
            break;
        }
        default:
        {
            message += "INIT_ERROR";
            clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_ERROR,
                            message,
                            clc_log::ELogWriter::MEGA_CHAT);
            break;
        }
    }
}

void CLCListener::onChatConnectionStateUpdate(c::MegaChatApi* api,
                                              c::MegaChatHandle chatid,
                                              int newState)
{
    using namespace clc_global;
    if (chatid == c::MEGACHAT_INVALID_HANDLE && newState == c::MegaChatApi::CHAT_CONNECTION_ONLINE)
    {

        clc_log::logMsg(m::logDebug,
                        "onChatConnectionStateUpdate connected to all chatrooms",
                        clc_log::ELogWriter::MEGA_CHAT);
        g_allChatsLoggedIn = true;
    }

    if (chatid == g_reviewPublicChatid && newState == c::MegaChatApi::CHAT_CONNECTION_ONLINE)
    {
        // flag used to know when we are connected to a specific chatroom. g_reviewPublicChatid must
        // be set to expected chatid before this callback is executed
        g_reviewedChatLoggedIn = true;
    }
}

#ifndef KARERE_DISABLE_WEBRTC
void CLCCallListener::onChatCallUpdate(megachat::MegaChatApi*, megachat::MegaChatCall* call)
{
    using namespace mclc::clc_global;
    clc_log::logMsg(m::logInfo, "Receiving a call update", clc_log::ELogWriter::MEGA_CHAT);
    if (!call)
    {
        clc_log::logMsg(m::logError, "onChatCallUpdate: NULL call", clc_log::ELogWriter::MEGA_CHAT);
        return;
    }
    megachat::MegaChatHandle chatid = call->getChatid();

    if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_STATUS))
    {
        clc_log::logMsg(m::logInfo,
                        "Call update: CHANGE_TYPE_STATUS",
                        clc_log::ELogWriter::MEGA_CHAT);
        int status = call->getStatus();
        auto findIt = g_callStateMap.find(chatid);
        if (status == megachat::MegaChatCall::CALL_STATUS_INITIAL)
        {
            if (findIt != g_callStateMap.end())
            {
                // This should not happen, the call should be destroyed before creating it again.
                int previousStatus = findIt->second.state;
                clc_log::logMsg(m::logError,
                                "The call is already registered. Previous state: " +
                                    std::to_string(previousStatus),
                                clc_log::ELogWriter::MEGA_CHAT);
                assert(false);
                findIt->second.state = megachat::MegaChatCall::CALL_STATUS_INITIAL;
                findIt->second.stateHasChanged = true;
            }
            else
            {
                g_callStateMap.emplace(std::make_pair(
                    chatid,
                    CLCStateChange{megachat::MegaChatCall::CALL_STATUS_INITIAL, true}));
            }
        }
        else if (status == megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS)
        {
            clc_log::logMsg(m::logInfo,
                            "Call update: CALL_STATUS_IN_PROGRESS",
                            clc_log::ELogWriter::MEGA_CHAT);
            if (findIt == g_callStateMap.end())
            {
                // This should be imposible, the call must start with CALL_STATUS_INITIAL so it must
                // be in the map
                clc_log::logMsg(m::logError,
                                "Chat must exists in the map at this point",
                                clc_log::ELogWriter::MEGA_CHAT);
                assert(false);
                findIt->second.stateHasChanged = false; // force a time out
                return;
            }
            findIt->second.state = megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS;
            findIt->second.stateHasChanged = true;
        }
        else if (status == megachat::MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION)
        {
            clc_log::logMsg(m::logInfo,
                            "Call update: CALL_STATUS_TERMINATING_USER_PARTICIPATION",
                            clc_log::ELogWriter::MEGA_CHAT);
            if (findIt == g_callStateMap.end())
            {
                clc_log::logMsg(m::logError,
                                "Call must exists in the map at this point",
                                clc_log::ELogWriter::MEGA_CHAT);
                assert(false);
                findIt->second.stateHasChanged = false; // force a time out
                return;
            }
            findIt->second.state =
                megachat::MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION;
            findIt->second.stateHasChanged = true;
        }
        else if (status == megachat::MegaChatCall::CALL_STATUS_DESTROYED)
        {
            clc_log::logMsg(m::logInfo,
                            "Call update: CALL_STATUS_DESTROYED",
                            clc_log::ELogWriter::MEGA_CHAT);
            g_callStateMap.erase(chatid); // remove if exists
        }
        else
        {
            clc_log::logMsg(m::logWarning,
                            "Unmanaged call status " + std::to_string(status),
                            clc_log::ELogWriter::MEGA_CHAT);
        }
    }
    else if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_LOCAL_AVFLAGS))
    {}
    else if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_RINGING_STATUS))
    {}
    else if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_OWN_PERMISSIONS))
    {}
    else if (call->hasChanged(megachat::MegaChatCall::CHANGE_TYPE_GENERIC_NOTIFICATION))
    {}
}

void CLCCallListener::onChatSessionUpdate(megachat::MegaChatApi*,
                                          megachat::MegaChatHandle chatid,
                                          megachat::MegaChatHandle callid,
                                          megachat::MegaChatSession* session)
{
    if (!session)
    {
        clc_log::logMsg(m::logError, "NULL session", clc_log::ELogWriter::MEGA_CHAT);
        return;
    }
    clc_log::logMsg(m::logInfo,
                    std::string("onChatSessionUpdate with chatid ") + std::to_string(chatid) +
                        " and callid " + std::to_string(callid),
                    clc_log::ELogWriter::MEGA_CHAT);
    bool isNewlyCreatedSession =
        (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_STATUS) &&
         session->getStatus() == megachat::MegaChatCall::CALL_STATUS_INITIAL);
    bool haveSessionOptionsChanged =
        session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS);
    if (isNewlyCreatedSession || haveSessionOptionsChanged)
    {
        askForParticipantVideo(chatid, callid, session);
    }

    if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_STATUS))
    {}
    else if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_REMOTE_AVFLAGS))
    {}
    else if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_SESSION_ON_LOWRES))
    {
        if (session->canRecvVideoLowRes())
        {
            clc_global::g_callVideoParticipants.updateParticipantLowResVideoState(
                callid,
                session->getClientid(),
                true);
        }
        else
        {
            clc_global::g_callVideoParticipants.removeParticipant(callid, session->getClientid());
        }
    }
    else if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_SESSION_ON_HIRES))
    {
        if (session->canRecvVideoHiRes())
        {
            clc_global::g_callVideoParticipants.updateParticipantHighResVideoState(
                callid,
                session->getClientid(),
                true);
        }
        else
        {
            clc_global::g_callVideoParticipants.removeParticipant(callid, session->getClientid());
        }
    }
    else if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_SESSION_ON_HOLD))
    {}
    else if (session->hasChanged(megachat::MegaChatSession::CHANGE_TYPE_PERMISSIONS))
    {}
}

void CLCCallListener::askForParticipantVideo(const megachat::MegaChatHandle chatid,
                                             const megachat::MegaChatHandle callid,
                                             const megachat::MegaChatSession* session) const
{
    if (addParticipantLowResVideo(chatid, callid, session))
    {
        return;
    }
    addParticipantHighResVideo(chatid, callid, session);
}

bool CLCCallListener::addParticipantLowResVideo(const megachat::MegaChatHandle chatid,
                                                const megachat::MegaChatHandle callid,
                                                const megachat::MegaChatSession* session) const
{
    if (!session->isLowResVideo())
    {
        return false;
    }
    int errCode = clc_global::g_callVideoParticipants.addLowResParticipant(
        callid,
        clc_report::ParticipantInfo{session->getClientid(), session->getPeerid(), false});
    if (errCode != c::MegaChatError::ERROR_OK)
    {
        return false;
    }
    auto users = std::unique_ptr<m::MegaHandleList>(m::MegaHandleList::createInstance());
    users->addMegaHandle(session->getClientid());

    clc_listen::CLCChatRequestTracker resListener(clc_global::g_chatApi.get());
    clc_global::g_chatApi->requestLowResVideo(chatid, users.get(), &resListener);
    return true;
}

bool CLCCallListener::addParticipantHighResVideo(const megachat::MegaChatHandle chatid,
                                                 const megachat::MegaChatHandle callid,
                                                 const megachat::MegaChatSession* session) const
{
    if (!session->isHiResVideo())
    {
        return false;
    }
    int errCode = clc_global::g_callVideoParticipants.addHighResParticipant(
        callid,
        clc_report::ParticipantInfo{session->getClientid(), session->getPeerid(), false});
    if (errCode != c::MegaChatError::ERROR_OK)
    {
        return false;
    }
    clc_listen::CLCChatRequestTracker resListener(clc_global::g_chatApi.get());
    clc_global::g_chatApi->requestHiResVideo(chatid, session->getClientid(), &resListener);
    return true;
}
#endif

void CLCChatListener::onFinish(int n, std::function<void(CLCFinishInfo&)> f)
{
    std::lock_guard<std::mutex> g(m);
    finishFn[n] = f;
}

void CLCChatListener::onRequestFinish(c::MegaChatApi* api,
                                      c::MegaChatRequest* request,
                                      c::MegaChatError* e)
{
    assert(request && e);
    std::function<void(CLCFinishInfo&)> f;
    {
        std::lock_guard<std::mutex> g(m);
        auto i = finishFn.find(request->getType());
        if (i != finishFn.end())
            f = i->second;
    }
    if (f)
    {
        CLCFinishInfo fi{api, request, e};
        f(fi);
    }
    else
    {
        switch (request->getType())
        {
            case c::MegaChatRequest::TYPE_SET_ONLINE_STATUS:
                if (clc_log::check_err("SetChatStatus", e))
                {}
                break;

            case c::MegaChatRequest::TYPE_SET_PRESENCE_AUTOAWAY:
                if (clc_log::check_err("SetAutoAway", e))
                {
                    clc_console::conlock(std::cout)
                        << " autoaway: " << request->getFlag()
                        << " timeout: " << request->getNumber() << std::endl;
                }
                break;

            case c::MegaChatRequest::TYPE_SET_PRESENCE_PERSIST:
                if (clc_log::check_err("SetPresencePersist", e))
                {
                    clc_console::conlock(std::cout)
                        << " persist: " << request->getFlag() << std::endl;
                }
                break;

            case c::MegaChatRequest::TYPE_SET_BACKGROUND_STATUS:
                if (clc_log::check_err("SetBackgroundStatus", e))
                {
                    clc_console::conlock(std::cout)
                        << " background: " << request->getFlag() << std::endl;
                }
                break;

            case c::MegaChatRequest::TYPE_LOAD_PREVIEW:
                if (clc_log::check_err("OpenChatPreview", e))
                {
                    clc_console::conlock(std::cout)
                        << "openchatpreview: chatlink loaded. Chatid: "
                        << karere::Id(request->getChatHandle()).toString() << std::endl;
                }
                break;
        }
    }
}

void CLCMegaListener::onUsersUpdate(m::MegaApi*, m::MegaUserList* users)
{
    clc_console::conlock(std::cout)
        << "User list updated:  " << (users ? users->size() : -1) << std::endl;
    if (users)
    {
        for (int i = 0; i < users->size(); ++i)
        {
            if (m::MegaUser* m = users->get(i))
            {
                auto changebits = m->getChanges();
                if (changebits)
                {
                    auto cl = clc_console::conlock(std::cout);
                    std::cout << "user " << str_utils::ch_s(m->getHandle()) << " changes:";
                    if (changebits & m::MegaUser::CHANGE_TYPE_AUTHRING)
                        std::cout << " AUTHRING";
                    if (changebits & m::MegaUser::CHANGE_TYPE_LSTINT)
                        std::cout << " LSTINT";
                    if (changebits & m::MegaUser::CHANGE_TYPE_AVATAR)
                        std::cout << " AVATAR";
                    if (changebits & m::MegaUser::CHANGE_TYPE_FIRSTNAME)
                        std::cout << " FIRSTNAME";
                    if (changebits & m::MegaUser::CHANGE_TYPE_LASTNAME)
                        std::cout << " LASTNAME";
                    if (changebits & m::MegaUser::CHANGE_TYPE_EMAIL)
                        std::cout << " EMAIL";
                    if (changebits & m::MegaUser::CHANGE_TYPE_KEYRING)
                        std::cout << " KEYRING";
                    if (changebits & m::MegaUser::CHANGE_TYPE_COUNTRY)
                        std::cout << " COUNTRY";
                    if (changebits & m::MegaUser::CHANGE_TYPE_BIRTHDAY)
                        std::cout << " BIRTHDAY";
                    if (changebits & m::MegaUser::CHANGE_TYPE_PUBKEY_CU255)
                        std::cout << " PUBKEY_CU255";
                    if (changebits & m::MegaUser::CHANGE_TYPE_PUBKEY_ED255)
                        std::cout << " PUBKEY_ED255";
                    if (changebits & m::MegaUser::CHANGE_TYPE_SIG_PUBKEY_RSA)
                        std::cout << " SIG_PUBKEY_RSA";
                    if (changebits & m::MegaUser::CHANGE_TYPE_SIG_PUBKEY_CU255)
                        std::cout << " SIG_PUBKEY_CU255";
                    if (changebits & m::MegaUser::CHANGE_TYPE_LANGUAGE)
                        std::cout << " LANGUAGE";
                    if (changebits & m::MegaUser::CHANGE_TYPE_PWD_REMINDER)
                        std::cout << " PWD_REMINDER";
                    if (changebits & m::MegaUser::CHANGE_TYPE_DISABLE_VERSIONS)
                        std::cout << " DISABLE_VERSIONS";
                    std::cout << std::endl;
                }
            }
        }
    }
}

void CLCMegaListener::onAccountUpdate(m::MegaApi*)
{
    clc_console::conlock(std::cout) << "Account updated" << std::endl;
}

void CLCMegaListener::onContactRequestsUpdate(m::MegaApi*, m::MegaContactRequestList* requests)
{
    clc_console::conlock(std::cout)
        << "Contact requests list updated:  " << (requests ? requests->size() : -1) << std::endl;
}

void CLCMegaListener::onReloadNeeded(m::MegaApi* api)
{
    {
        clc_console::conlock(std::cout)
            << "Reload needed!  Submitting fetchNodes request" << std::endl;
    }
    api->fetchNodes();
}

#ifdef ENABLE_SYNC
void CLCMegaListener::onGlobalSyncStateChanged(m::MegaApi*)
{
    clc_console::conlock(std::cout) << "Sync state changed";
}
#endif

void CLCMegaListener::onChatsUpdate(m::MegaApi*, m::MegaTextChatList* chats)
{
    clc_console::conlock(std::cout)
        << "Chats updated:  " << (chats ? chats->size() : -1) << std::endl;
}

void CLCMegaListener::onEvent(m::MegaApi*, m::MegaEvent* e)
{
    if (e)
    {
        LOG_info << "Event: " << e->getEventString();
        LOG_info << "\tText: " << (e->getText() ? e->getText() : "(null)");
        LOG_info << "\tNumber: " << std::to_string(e->getNumber());
        LOG_info << "\tHandle: " << m::Base64Str<sizeof(m::MegaHandle)>(e->getHandle());
    }
    else
    {
        assert(false);
    }
}

void CLCMegaListener::onRequestFinish(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e)
{
    using namespace mclc::clc_global;

    std::unique_lock<std::mutex> guard(g_outputMutex);
    switch (request->getType())
    {
        case m::MegaRequest::TYPE_LOGIN:
            if (clc_log::check_err("Login", e))
            {
                clc_console::conlock(std::cout)
                    << "Loading Account with fetchNodes..." << std::endl;
                guard.unlock();
                api->fetchNodes();
                setprompt(clc_prompt::NOPROMPT);
            }
            else if (e->getErrorCode() == m::MegaError::API_EMFAREQUIRED)
            {
                guard.unlock();
                setprompt(clc_prompt::PIN);
            }
            else
            {
                guard.unlock();
                setprompt(clc_prompt::COMMAND);
            }

            g_dumpHistoryChatid = c::MEGACHAT_INVALID_HANDLE;
            g_reviewingPublicChat = false;
            g_dumpingChatHistory = false;
            break;
        case m::MegaRequest::TYPE_FETCH_NODES:
            if (clc_log::check_err("FetchNodes", e))
            {
                clc_console::conlock(std::cout) << "Connecting to chat servers" << std::endl;
                guard.unlock();

                setprompt(clc_prompt::COMMAND);
            }
            break;

        case m::MegaRequest::TYPE_LOGOUT:
            if (!clc_log::check_err("Logout", e))
            {
                clc_console::conlock(std::cout)
                    << "Error in logout: " << e->getErrorString() << std::endl;
            }

            g_dumpHistoryChatid = c::MEGACHAT_INVALID_HANDLE;
            g_reviewingPublicChat = false;
            g_dumpingChatHistory = false;
            guard.unlock();
            setprompt(clc_prompt::COMMAND);

        default:
            break;
    }
}

void CLCRoomListener::onChatRoomUpdate(megachat::MegaChatApi*, megachat::MegaChatRoom* chat)
{
    clc_log::logMsg(c::MegaChatApi::LOG_LEVEL_INFO,
                    "Room " + str_utils::ch_s(chat->getChatId()) + " updated",
                    clc_log::ELogWriter::MEGA_CHAT);
}

void CLCRoomListener::onMessageLoaded(megachat::MegaChatApi*, megachat::MegaChatMessage* msg)
{
    clc_report::reportMessage(room, msg, "loaded");
}

void CLCRoomListener::onMessageReceived(megachat::MegaChatApi*, megachat::MegaChatMessage*) {}

void CLCRoomListener::onMessageUpdate(megachat::MegaChatApi*, megachat::MegaChatMessage*) {}

void CLCRoomListener::onHistoryReloaded(megachat::MegaChatApi*, megachat::MegaChatRoom*) {}

void CLCRoomListener::onHistoryTruncatedByRetentionTime(c::MegaChatApi*, c::MegaChatMessage*) {}

CLCChatRequestTracker::~CLCChatRequestTracker()
{
    if (!resultReceived)
    {
        mMegaChatApi->removeChatRequestListener(this);
    }
}

void CLCChatRequestTracker::onRequestFinish(::megachat::MegaChatApi*,
                                            ::megachat::MegaChatRequest* req,
                                            ::megachat::MegaChatError* e)
{
    request.reset(req ? req->copy() : nullptr);
    error.reset(e ? e->copy() : nullptr);
    finish(e->getErrorCode(), e->getErrorString() ? e->getErrorString() : "");
}

}
