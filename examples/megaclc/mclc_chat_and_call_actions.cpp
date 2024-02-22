#include "mclc_chat_and_call_actions.h"

#include "mclc_globals.h"
#include "mclc_listeners.h"
#include "mclc_logging.h"

#include <mega.h>

namespace mclc::clc_ccactions
{

using mclc::clc_global::g_chatApi;
using mclc::clc_log::ELogWriter;
using mclc::clc_log::logMsg;

namespace // Private utilities
{

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
    auto callNotificationRecv = megachat::async::waitForResponse(hasCallStateChanged, 60);
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
        logMsg(m::logError, "Call cannot be retrieved for chatid", ELogWriter::MEGA_CHAT);
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
};

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

}
