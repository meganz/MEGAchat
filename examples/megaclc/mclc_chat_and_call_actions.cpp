#include "mclc_chat_and_call_actions.h"

#include "mclc_globals.h"
#include "mclc_listeners.h"
#include "mclc_logging.h"

#include <chrono>
#include <mega.h>

namespace mclc::clc_ccactions
{

using mclc::clc_global::g_chatApi;
using mclc::clc_log::ELogWriter;
using mclc::clc_log::logMsg;

namespace // Private utilities
{
using namespace std::chrono_literals;

constexpr auto ONE_MINUTE_SECS = 60s;
constexpr auto TEN_SECS = 10s;

#ifndef KARERE_DISABLE_WEBRTC
struct CallStateChangeTracker
{
    c::MegaChatHandle chatId{c::MEGACHAT_INVALID_HANDLE};

    bool operator()()
    {
        if (chatId == megachat::MEGACHAT_INVALID_HANDLE)
        {
            return false;
        }

        auto it = clc_global::g_callStateMap.find(chatId);
        if (it == clc_global::g_callStateMap.end())
        {
            return false;
        }
        return it->second.stateHasChanged;
    }

    /**
     * @brief Return current call status received at onChatCallUpdate
     *
     * @return This method will return a pair with <errorCode, callStatus>:
     * - <ERROR_ARGS, CALL_STATUS_INITIAL> if chatId is invalid
     * - <ERROR_NOENT, CALL_STATUS_INITIAL> if chatId is not found at g_callStateMap
     * - <ERROR_OK, callStatus at g_callStateMap> if stateHasChanged is true
     * - <ERROR_EXIST, callStatus at g_callStateMap> if stateHasChanged is false
     */
    std::pair<int, int> getCurrentCallstatus()
    {
        if (chatId == megachat::MEGACHAT_INVALID_HANDLE)
        {
            return std::make_pair(megachat::MegaChatError::ERROR_ARGS,
                                  megachat::MegaChatCall::CALL_STATUS_INITIAL);
        }

        auto it = clc_global::g_callStateMap.find(chatId);
        if (it == clc_global::g_callStateMap.end())
        {
            return std::make_pair(megachat::MegaChatError::ERROR_NOENT,
                                  megachat::MegaChatCall::CALL_STATUS_INITIAL);
        }

        int errCode = it->second.stateHasChanged ? megachat::MegaChatError::ERROR_OK :
                                                   megachat::MegaChatError::ERROR_EXIST;

        return std::make_pair(errCode, it->second.state.load());
    }
};

bool waitForReceivingCallStatus(const c::MegaChatHandle chatId,
                                const std::set<int>& allowedExpectedStatuses)
{
    if (chatId == megachat::MEGACHAT_INVALID_HANDLE)
    {
        return false;
    }

    // wait for receiving (CHANGE_TYPE_STATUS)
    CallStateChangeTracker hasCallStateChanged{chatId};
    auto callNotificationRecv =
        megachat::async::waitForResponse(hasCallStateChanged, ONE_MINUTE_SECS);
    if (!callNotificationRecv)
    {
        // if call already exists at this point this notification won't be received (i.e this
        // command is executed more than once)
        logMsg(m::logWarning,
               "Timeout expired for received notification about new call",
               ELogWriter::MEGA_CHAT);
        return false;
    }

    std::unique_ptr<megachat::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (!call)
    {
        // if no call no sense to continue with command processing
        logMsg(m::logError,
               "Call cannot be retrieved for chatid: " + std::to_string(chatId),
               ELogWriter::MEGA_CHAT);
        return false;
    }

    return allowedExpectedStatuses.find(call->getStatus()) != allowedExpectedStatuses.end();
}

bool resetCallStateChangeRecv(const c::MegaChatHandle chatId, const bool v)
{
    if (chatId == megachat::MEGACHAT_INVALID_HANDLE)
    {
        return false;
    }

    auto it = clc_global::g_callStateMap.find(chatId);
    if (it == clc_global::g_callStateMap.end())
    {
        return false;
    }
    it->second.stateHasChanged = v;
    return true;
}

/**
 * @brief This method returns true if call in chatroom represented by chatId is still alive
 * (CALL_STATUS_IN_PROGRESS)
 *
 * @param chatId The chat handle that identifies chatroom
 * @return true if call in chatroom represented by chatId is still alive, otherwise false
 */
bool isCallAlive(const c::MegaChatHandle chatId)
{
    const bool sdkLoggedIn = clc_global::g_megaApi->isLoggedIn();
    if (!sdkLoggedIn)
    {
        logMsg(m::logError, "Sdk is not logged in", ELogWriter::MEGA_CHAT);
        return false;
    }

    auto expStatus = c::MegaChatCall::CALL_STATUS_IN_PROGRESS;
    std::unique_ptr<megachat::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (!call || call->getStatus() != expStatus)
    {
        logMsg(m::logError,
               "Cannot get call or it's status is unexpected" + std::to_string(expStatus),
               ELogWriter::MEGA_CHAT);
        return false;
    }

    CallStateChangeTracker callChanged{chatId};
    auto p = callChanged.getCurrentCallstatus();
    if (p.first != megachat::MegaChatError::ERROR_OK ||
        p.second != c::MegaChatCall::CALL_STATUS_IN_PROGRESS)
    {
        logMsg(m::logError,
               "Unexpected call state received at onChatCallUpdate",
               ELogWriter::MEGA_CHAT);
        return false;
    }

    logMsg(m::logDebug, "Call is still alive", ELogWriter::MEGA_CHAT);
    if (clc_global::g_callVideoParticipants.isValid())
    {
        logMsg(m::logDebug,
               clc_global::g_callVideoParticipants.receivingVideoReport(),
               ELogWriter::MEGA_CHAT);
    }
    return true;
}
#endif // KARERE_DISABLE_WEBRTC

bool hasMegaChatBeenInit()
{
    return clc_global::g_chatApi->getInitState() != c::MegaChatApi::INIT_NOT_DONE;
}

bool areWeLoggedInSdk()
{
    return clc_global::g_megaApi->isLoggedIn();
}

bool areWeLoggedIn()
{
    return areWeLoggedInSdk() || hasMegaChatBeenInit();
}

bool areWeInAnonymousMode()
{
    return clc_global::g_chatApi->getInitState() == c::MegaChatApi::INIT_ANONYMOUS;
}

bool areWeLoggedOut()
{
    return !(areWeLoggedIn() || areWeInAnonymousMode());
}

} // end of namespace // Private utilities

bool logoutFromAnonymousMode()
{
    if (!areWeInAnonymousMode())
    {
        return false;
    }
    clc_global::g_chatFinishedLogout = false;
    clc_listen::CLCChatRequestTracker logoutTracker(g_chatApi.get());
    clc_global::g_chatApi->logout(&logoutTracker);
    bool logoutSucess = logoutTracker.waitForResult() == megachat::MegaChatError::ERROR_OK;
    if (!logoutSucess)
    {
        logMsg(m::logError, "Unable to log out from anonymous mode", ELogWriter::MEGA_CHAT);
    }
    logoutSucess = c::async::waitForResponse(
        []()
        {
            return clc_global::g_chatFinishedLogout.load();
        },
        ONE_MINUTE_SECS);
    if (!logoutSucess)
    {
        logMsg(m::logError,
               "Unable to log out the chats from anonymous mode",
               ELogWriter::MEGA_CHAT);
    }
    return logoutSucess;
}

bool sdkLogout()
{
    clc_global::g_chatFinishedLogout = false;
    setprompt(clc_prompt::NOPROMPT);

    clc_listen::OneShotRequestTracker logoutTracker(clc_global::g_megaApi.get());
#ifdef ENABLE_SYNC
    clc_global::g_megaApi->logout(false, &logoutTracker);
#else
    clc_global::g_megaApi->logout(&logoutTracker);
#endif

    bool isMegaChatLoggedOut = c::async::waitForResponse(
        []()
        {
            return clc_global::g_chatFinishedLogout.load();
        },
        TEN_SECS);
    if (!isMegaChatLoggedOut)
    {
        logMsg(m::logError, "Unable to log out chat api", ELogWriter::MEGA_CHAT);
    }
    bool chatLogoutSucess = logoutTracker.waitForResult(10) == megachat::MegaChatError::ERROR_OK;
    if (!chatLogoutSucess)
    {
        logMsg(m::logError, "Unable to log out from MEGAChat", ELogWriter::SDK);
    }
    return chatLogoutSucess && isMegaChatLoggedOut;
}

bool logout()
{
    std::unique_ptr<const char[]> session(clc_global::g_megaApi->dumpSession());
    if (areWeInAnonymousMode())
    {
        return logoutFromAnonymousMode();
    }
    if (!areWeLoggedIn())
    {
        logMsg(m::logError, "You are trying to logout without being logged in", ELogWriter::SDK);
        return false;
    }
    return sdkLogout();
}

bool ensureLogout()
{
    if (areWeLoggedOut())
    {
        logMsg(m::logDebug, "Already logged out, nothing to do", ELogWriter::SDK);
        return true;
    }
    return logout();
}

bool login(const char* email, const char* password)
{
    if (clc_global::g_megaApi->isLoggedIn())
    {
        logMsg(m::logError,
               "You are already logged in. Please logout before trying again.",
               ELogWriter::SDK);
        return false;
    }
    clc_global::g_chatApi->init(NULL);
    clc_global::g_login = email;
    clc_global::g_password = password;
    clc_global::g_allChatsLoggedIn = false;

    // Login request
    clc_listen::OneShotRequestTracker loginTracker(clc_global::g_megaApi.get());
    clc_global::g_megaApi->login(email, password, &loginTracker);
    if (int errCode = loginTracker.waitForResult(); errCode != m::API_OK)
    {
        logMsg(m::logError,
               std::string("ERROR CODE ") + std::to_string(errCode) + ": Failed to log in",
               ELogWriter::SDK);
        return false;
    }
    // Fetch nodes
    bool hasFetchNodesFinished = c::async::waitForResponse(
        []()
        {
            // While fetching nodes g_prompt == NOPROMPT. Once it finishes COMMAND
            return clc_global::g_prompt == clc_prompt::COMMAND;
        },
        ONE_MINUTE_SECS);
    if (!hasFetchNodesFinished)
    {
        logMsg(m::logError,
               "Time limit exceed to complete the login process, maybe too many nodes to fetch.",
               ELogWriter::SDK);
        return false;
    }
    // All chats logged in
    bool hasAllChatsBeenLoggedIn = c::async::waitForResponse(
        []()
        {
            return clc_global::g_allChatsLoggedIn.load();
        },
        ONE_MINUTE_SECS);
    if (!hasAllChatsBeenLoggedIn)
    {
        logMsg(m::logError,
               "Time limit exceed to login all the chats of the account",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}

std::pair<c::MegaChatHandle, int> openChatLink(const std::string& link)
{
    auto unexpectedInitState =
        g_chatApi->getInitState() != megachat::MegaChatApi::INIT_ONLINE_SESSION &&
        g_chatApi->getInitState() != megachat::MegaChatApi::INIT_ANONYMOUS;
    if (unexpectedInitState)
    {
        logMsg(m::logError,
               "Your init state in MegaChat is not appropiate to open a chat link",
               ELogWriter::SDK);
        return {c::MEGACHAT_INVALID_HANDLE, -999};
    }

    clc_listen::CLCChatRequestTracker openPreviewListener(g_chatApi.get());
    g_chatApi->openChatPreview(link.c_str(), &openPreviewListener);
    int errCode = openPreviewListener.waitForResult();
    bool openPreviewSuccess = errCode == megachat::MegaChatError::ERROR_EXIST ||
                              errCode == megachat::MegaChatError::ERROR_OK;
    if (!openPreviewSuccess)
    {
        logMsg(m::logError,
               std::string("ERROR CODE ") + std::to_string(errCode) + ": Failed to open chat link.",
               ELogWriter::SDK);
        return {c::MEGACHAT_INVALID_HANDLE, errCode};
    }
    c::MegaChatHandle chatId = openPreviewListener.getMegaChatRequestPtr()->getChatHandle();
    std::unique_ptr<c::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(chatId));
    if (!chatRoom)
    {
        logMsg(m::logError,
               "We are not able to get the chat room although it should exist",
               ELogWriter::SDK);
        return {c::MEGACHAT_INVALID_HANDLE, errCode};
    }
    return {chatId, errCode};
}

bool joinChat(const c::MegaChatHandle chatId, const int openPreviewErrCode)
{
    std::unique_ptr<c::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(chatId));
    if (!chatRoom)
    {
        logMsg(m::logError,
               "We are not able to get the chat with the given id",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    bool continueWithAutoJoin =
        chatRoom->isPreview() || openPreviewErrCode == megachat::MegaChatError::ERROR_OK;
    if (continueWithAutoJoin)
    {
        logMsg(m::logInfo, "### Autojoin chat ###", ELogWriter::MEGA_CHAT);
        clc_listen::CLCChatRequestTracker autoJoinListener(g_chatApi.get());
        g_chatApi->autojoinPublicChat(chatId, &autoJoinListener);
        if (clc_log::isUnexpectedErr(autoJoinListener.waitForResult(),
                                     megachat::MegaChatError::ERROR_OK,
                                     "Failed autoJoin the chat",
                                     ELogWriter::MEGA_CHAT))
        {
            return false;
        }
    }
    else if (chatRoom->getOwnPrivilege() == megachat::MegaChatRoom::PRIV_RM)
    {
        logMsg(m::logInfo, "### Autorejoin chat ###", ELogWriter::MEGA_CHAT);
        clc_listen::CLCChatRequestTracker autoReJoinListener(g_chatApi.get());
        g_chatApi->autorejoinPublicChat(chatId, chatRoom->getChatId(), &autoReJoinListener);
        if (clc_log::isUnexpectedErr(autoReJoinListener.waitForResult(),
                                     megachat::MegaChatError::ERROR_OK,
                                     "Failed autoReJoin the chat",
                                     ELogWriter::MEGA_CHAT))
        {
            return false;
        }
    }
    else if (chatRoom->getOwnPrivilege() > megachat::MegaChatRoom::PRIV_RM)
    {
        logMsg(m::logInfo, "### Already joined ###", ELogWriter::MEGA_CHAT);
        logMsg(m::logWarning,
               "You are trying to join a chat that you were already joined",
               ELogWriter::MEGA_CHAT);
    }
    else
    {
        logMsg(m::logError, "### Unexpected use case ###", ELogWriter::MEGA_CHAT);
        assert(false);
        return false;
    }
    return true;
}

#ifndef KARERE_DISABLE_WEBRTC
bool waitUntilCallIsReceived(const c::MegaChatHandle chatId)
{
    std::set<int> expStatus{
        megachat::MegaChatCall::CALL_STATUS_INITIAL,
        megachat::MegaChatCall::CALL_STATUS_USER_NO_PRESENT,
        megachat::MegaChatCall::
            CALL_STATUS_TERMINATING_USER_PARTICIPATION // this last one could be removed if we set
                                                       // as requirement to be logged out to execute
                                                       // exec_joinCallViaMeetingLink
    };
    logMsg(m::logDebug, "Chat id: " + std::to_string(chatId), ELogWriter::MEGA_CHAT);
    std::unique_ptr<megachat::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (call && expStatus.find(call->getStatus()) == expStatus.end())
    {
        logMsg(m::logDebug, "Call is in unexpected state", ELogWriter::MEGA_CHAT);
        return false;
    }

    if (!call && !waitForReceivingCallStatus(chatId, expStatus))
    {
        logMsg(m::logError, "Call cannot be retrieved for chatid", ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}

bool startChatCall(const c::MegaChatHandle chatId,
                   const bool audio,
                   const bool video,
                   const bool notRinging)
{
    clc_listen::CLCChatRequestTracker startChatCallListener(g_chatApi.get());
    g_chatApi->startCallInChat(chatId, video, audio, notRinging, &startChatCallListener);
    if (clc_log::isUnexpectedErr(startChatCallListener.waitForResult(),
                                 megachat::MegaChatError::ERROR_OK,
                                 "Failed to start the call",
                                 ELogWriter::MEGA_CHAT))
    {
        return false;
    }
    if (!waitForReceivingCallStatus(chatId, {megachat::MegaChatCall::CALL_STATUS_IN_PROGRESS}))
    {
        logMsg(m::logError, "Unexpected call status", ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}

int waitInCallFor(const c::MegaChatHandle chatId, const unsigned int waitTimeSec)
{
    auto endAt = ::mega::m_time(nullptr) + waitTimeSec;
    while (true)
    {
        auto now = ::mega::m_time(nullptr);
        auto timeoutExpired = waitTimeSec != callUnlimitedDuration && now >= endAt;
        if (timeoutExpired)
        {
            return megachat::MegaChatError::ERROR_OK;
        }

        if (!isCallAlive(chatId))
        {
            return megachat::MegaChatError::ERROR_NOENT;
        }
        clc_time::WaitMillisec(callIsAliveMillis);
    }
}

bool answerCall(const c::MegaChatHandle chatId,
                const bool audio,
                const bool video,
                const std::set<int>& expectedStatus)
{
    if (!resetCallStateChangeRecv(chatId, false))
    {
        logMsg(m::logError, "Cannot update stateHasChanged for ...", ELogWriter::MEGA_CHAT);
        return false;
    }
    std::unique_ptr<c::MegaChatRoom> chatRoom(g_chatApi->getChatRoom(chatId));
    if (!chatRoom)
    {
        logMsg(m::logError,
               "We are not able to get the chat with the given id",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    clc_listen::CLCChatRequestTracker answerChatCallListener(g_chatApi.get());
    g_chatApi->answerChatCall(chatRoom->getChatId(), video, audio, &answerChatCallListener);
    if (clc_log::isUnexpectedErr(answerChatCallListener.waitForResult(),
                                 megachat::MegaChatError::ERROR_OK,
                                 "Failed to answer the call",
                                 ELogWriter::MEGA_CHAT))
    {
        return false;
    }

    if (!waitForReceivingCallStatus(chatId, expectedStatus))
    {
        logMsg(m::logError, "Unexpected call status", ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}

bool hangUpCall(const c::MegaChatHandle chatId)
{
    if (!resetCallStateChangeRecv(chatId, false))
    {
        logMsg(m::logError, "Unexpected call state after answering", ELogWriter::MEGA_CHAT);
        return false;
    }
    std::set<int> expStatus = {megachat::MegaChatCall::CALL_STATUS_USER_NO_PRESENT,
                               megachat::MegaChatCall::CALL_STATUS_TERMINATING_USER_PARTICIPATION,
                               megachat::MegaChatCall::CALL_STATUS_DESTROYED};

    clc_listen::CLCChatRequestTracker hangUpListener(g_chatApi.get());
    std::unique_ptr<megachat::MegaChatCall> call(g_chatApi->getChatCall(chatId));
    if (!call)
    {
        logMsg(m::logError,
               "Cannot hangup call, as it doesn't exists at this point",
               ELogWriter::MEGA_CHAT);
        return false;
    }

    g_chatApi->hangChatCall(call->getCallId(), &hangUpListener);
    if (clc_log::isUnexpectedErr(hangUpListener.waitForResult(),
                                 megachat::MegaChatError::ERROR_OK,
                                 "Failed to answer hang up the call",
                                 ELogWriter::MEGA_CHAT))
    {
        return false;
    }

    if (!waitForReceivingCallStatus(chatId, expStatus))
    {
        logMsg(m::logError,
               "CALL_STATUS_TERMINATING_USER_PARTICIPATION not received",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}

bool setChatVideoInDevice(const std::string& device)
{
    clc_listen::CLCChatRequestTracker setInputListener(g_chatApi.get());
    g_chatApi->setChatVideoInDevice(device.c_str(), &setInputListener);
    auto errorCode = setInputListener.waitForResult();
    if (errorCode == c::MegaChatError::ERROR_ARGS)
    {
        std::ostringstream msg;
        msg << "setChatVideoInDevice: the input device (";
        msg << device;
        msg << ") is not a valid device. ";
        std::unique_ptr<m::MegaStringList> availableDevices(g_chatApi->getChatVideoInDevices());
        if (availableDevices->size() == 0)
        {
            msg << "There are no available input devices.";
        }
        else
        {
            msg << "The available ones are:\n";
            msg << str_utils::joinStringList(*availableDevices, ", ");
        }
        msg << "\n";
        logMsg(m::logError, msg.str(), ELogWriter::MEGA_CHAT);
        return false;
    }
    if (errorCode == c::MegaChatError::ERROR_ACCESS)
    {
        logMsg(m::logError,
               "setChatVideoInDevice: WebRTC is not initialized. Initialize it before setting the "
               "input device.",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    if (errorCode != c::MegaChatError::ERROR_OK)
    {
        logMsg(m::logError,
               "setChatVideoInDevice: Unexpected error code (" + std::to_string(errorCode) + ")",
               ELogWriter::MEGA_CHAT);
        return false;
    }
    return true;
}
#endif // KARERE_DISABLE_WEBRTC
} // end of namespace mclc::clc_ccactions
