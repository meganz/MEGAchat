#include "mclc_reports.h"

#include "mclc_enums_to_string.h"
#include "mclc_general_utils.h"
#include "mclc_globals.h"
#include "mclc_listeners.h"
#include "mclc_logging.h"

#include <iostream>

namespace mclc::clc_report
{

using namespace mclc::clc_global;

namespace
{

bool oneOpenRoom(c::MegaChatHandle room)
{
    return g_roomListeners.size() == 1 && g_roomListeners.begin()->first == room;
}

}

void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid)
{
    int source;
    if (g_chatApi->isFullHistoryLoaded(chatid))
    {
        source = c::MegaChatApi::SOURCE_NONE;
    }
    else
    {
        int numberOfMessages = g_reviewChatMsgCountRemaining.load() > 0 ?
                                   g_reviewChatMsgCountRemaining.load() :
                                   MAX_NUMBER_MESSAGES;
        source = g_chatApi->loadMessages(chatid, numberOfMessages);
    }

    auto cl = clc_console::conlock(std::cout);
    switch (source)
    {
        case c::MegaChatApi::SOURCE_ERROR:
        {
            std::cout << "Loading messages..." << std::endl;
            break;
        }
        case c::MegaChatApi::SOURCE_NONE:
        {
            std::string message =
                "No more messages. Message loaded: " + std::to_string(g_reviewChatMsgCount);
            clc_console::conlock(std::cout) << message << std::flush;
            if (g_reviewPublicChatOutFile)
            {
                clc_console::conlock(*g_reviewPublicChatOutFile) << message << std::flush;
            }

            g_dumpingChatHistory = false;
            g_reviewingPublicChat = false;
            g_reviewChatMsgCountRemaining = 0;
            g_reviewChatMsgCount = 0;
            g_startedPublicChatReview = false;
            g_dumpHistoryChatid = c::MEGACHAT_INVALID_HANDLE;
            return;
        }
        default:
            return;
    }
}

void reportMessageHuman(c::MegaChatHandle chatid,
                        c::MegaChatMessage* msg,
                        const char* loadorreceive)
{
    if (!msg)
    {
        std::cout << "Room " << str_utils::ch_s(chatid) << " - end of " << loadorreceive
                  << " messages" << std::endl;
        if ((g_reviewingPublicChat || g_dumpingChatHistory) && g_reviewChatMsgCountRemaining)
        {
            reviewPublicChatLoadMessages(chatid);
        }
        else
        {
            std::string message =
                "Loaded all messages requested: " + std::to_string(g_reviewChatMsgCount);
            clc_console::conlock(std::cout) << message << std::flush;
            if (g_reviewPublicChatOutFile)
            {
                clc_console::conlock(*g_reviewPublicChatOutFile) << message << std::flush;
            }
        }
        return;
    }

    if ((g_reviewingPublicChat || g_dumpingChatHistory) && g_reviewChatMsgCountRemaining > 0)
    {
        --g_reviewChatMsgCountRemaining;
    }

    g_reviewChatMsgCount++;

    const std::shared_ptr<c::MegaChatRoom> room(g_chatApi->getChatRoom(chatid));
    const std::string room_title = room ? room->getTitle() : "<No Title>";

    auto firstname = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char[]> firstnamePtr(
            room ? g_chatApi->getUserFirstnameFromCache(handle) : nullptr);
        if (firstnamePtr)
        {
            return firstnamePtr.get();
        }

        return std::string{"<No Firstname>"};
    };

    auto lastname = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char[]> lastnamePtr(
            room ? g_chatApi->getUserLastnameFromCache(handle) : nullptr);
        if (lastnamePtr)
        {
            return lastnamePtr.get();
        }

        return std::string{"<No Lastname>"};
    };

    auto email = [room](const c::MegaChatHandle handle) -> std::string
    {
        std::unique_ptr<const char[]> emailPtr(room ? g_chatApi->getUserEmailFromCache(handle) :
                                                      nullptr);
        if (emailPtr)
        {
            return emailPtr.get();
        }

        return std::string{"<No Email>"};
    };

    auto nodeinfo = [](m::MegaNodeList* list)
    {
        if (!list || list->size() == 0)
        {
            return std::string{"<No Attachement>"};
        }
        std::stringstream ss;
        for (int i = 0; i < list->size(); ++i)
        {
            const auto node = list->get(i);
            ss << node->getName() << "(" << node->getSize() << ")";
            if (i + 1 < list->size())
            {
                ss << ", ";
            }
        }
        return ss.str();
    };

    auto metainfo = [](const c::MegaChatContainsMeta* meta)
    {
        if (!meta || !meta->getTextMessage())
        {
            return "<No Meta>";
        }
        return meta->getTextMessage();
    };

    auto callinfo = [](const int msgType, const int duration, const int termCode)
    {
        if (msgType != c::MegaChatMessage::TYPE_CALL_ENDED)
        {
            return std::string{"<Not an ending call>"};
        }
        return "Call ended: " + std::string(::megachat::MegaChatCall::termcodeToString(termCode)) +
               " - " + std::to_string(duration);
    };

    std::ostringstream os;
    os << room_title << " | " << clc_time::timeToStringUTC(msg->getTimestamp()) << " UTC"
       << " | " << clc_etos::msgTypeToString(msg->getType()) << " | "
       << str_utils::ch_s(msg->getMsgId()) << " | " << str_utils::ch_s(msg->getHandleOfAction())
       << " | " << str_utils::ch_s(msg->getUserHandle()) << " | "
       << (msg->hasConfirmedReactions() ? "reacted to" : "not reacted to") << " | "
       << (msg->isEdited() ? "edited" : "not edited") << " | "
       << (msg->isDeleted() ? "deleted" : "not deleted") << " | "
       << nodeinfo(msg->getMegaNodeList()) << " | " << metainfo(msg->getContainsMeta()) << " | "
       << callinfo(msg->getType(), msg->getDuration(), msg->getTermCode()) << " | "
       << firstname(msg->getUserHandle()) << " | " << lastname(msg->getUserHandle()) << " | "
       << email(msg->getUserHandle()) << " | "
       << (msg->getContent() ? msg->getContent() : "<No Content>") << std::endl;
    const auto outMsg = os.str();

    if (g_reviewPublicChatOutFileLinks && msg->getContent())
    {
        const auto subChatLink = str_utils::extractChatLink(msg->getContent());
        if (!subChatLink.empty())
        {
            clc_console::conlock(*g_reviewPublicChatOutFileLinks) << outMsg << std::flush;
        }
    }

    clc_console::conlock(std::cout) << outMsg;
    if (g_reviewPublicChatOutFile)
    {
        clc_console::conlock(*g_reviewPublicChatOutFile) << outMsg << std::flush;
    }
}

void reportMessage(c::MegaChatHandle chatid, c::MegaChatMessage* msg, const char* loadorreceive)
{
    if (!g_reportMessagesDeveloper)
    {
        reportMessageHuman(chatid, msg, loadorreceive);
        return;
    }

    auto cl = clc_console::conlock(std::cout);

    if (!msg)
    {
        std::cout << "Room " << str_utils::ch_s(chatid) << " - end of " << loadorreceive
                  << " messages" << std::endl;
        return;
    }

    if (!g_detailHigh && msg->getType() == c::MegaChatMessage::TYPE_NORMAL && msg->getContent())
    {
        std::cout << str_utils::ch_s(msg->getUserHandle());
        if (!oneOpenRoom(chatid))
        {
            std::cout << " (room " << str_utils::ch_s(chatid) << ")";
        }
        std::cout << ": " << msg->getContent() << std::endl;
        return;
    }

    std::cout << "Room " << str_utils::ch_s(chatid) << " " << loadorreceive << " message "
              << msg->getMsgIndex() << " from " << str_utils::ch_s(msg->getUserHandle())
              << " type: ";

    std::cout << clc_etos::msgTypeToString(msg->getType());

    if (msg->getMsgId() != c::MEGACHAT_INVALID_HANDLE)
    {
        std::cout << " id " << str_utils::ch_s(msg->getMsgId());
    }

    if (msg->getTempId() != c::MEGACHAT_INVALID_HANDLE)
    {
        std::cout << " tempid " << str_utils::ch_s(msg->getTempId());
    }

    if (msg->getRowId() != c::MEGACHAT_INVALID_HANDLE)
    {
        std::cout << " (manual row id " << str_utils::ch_s(msg->getRowId()) << ")";
    }
    if (msg->getChanges() != 0)
    {
        std::cout << " (change flags: " << msg->getChanges()
                  << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_STATUS) ? " status" : "")
                  << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_CONTENT) ? " content" : "")
                  << (msg->hasChanged(c::MegaChatMessage::CHANGE_TYPE_ACCESS) ? " access" : "")
                  << ")";
    }

    std::cout << std::endl
              << "content: '" << (msg->getContent() ? msg->getContent() : "<Null>")
              << "' status: " << msg->getStatus() << " timestamp " << msg->getTimestamp()
              << (msg->isEdited() ? " (edited)" : "") << (msg->isDeleted() ? " (deleted)" : "")
              << (msg->isEditable() ? " (editable)" : "")
              << (msg->isDeletable() ? " (deletable)" : "");

    if (msg->isManagementMessage())
    {
        std::cout << " (management, user " << str_utils::ch_s(msg->getHandleOfAction())
                  << " privilege " << c::MegaChatRoom::privToString(msg->getPrivilege()) << ")";
    }

    if (msg->getCode() != 0)
    {
        std::cout << " (reason: ";
        switch (msg->getCode())
        {
            case c::MegaChatMessage::REASON_PEERS_CHANGED:
                std::cout << "REASON_PEERS_CHANGED";
                break;
            case c::MegaChatMessage::REASON_TOO_OLD:
                std::cout << "REASON_TOO_OLD";
                break;
            case c::MegaChatMessage::REASON_GENERAL_REJECT:
                std::cout << "REASON_GENERAL_REJECT";
                break;
            case c::MegaChatMessage::REASON_NO_WRITE_ACCESS:
                std::cout << "REASON_NO_WRITE_ACCESS";
                break;
            case c::MegaChatMessage::REASON_NO_CHANGES:
                std::cout << "REASON_NO_CHANGES";
                break;
            default:
                std::cout << msg->getCode();
        }
        std::cout << ")";
    }

    if (msg->getUsersCount() > 0)
    {
        std::cout << " (attached users: " << msg->getUsersCount() << ")";
    }
    std::cout << std::endl;
}

int CLCCallReceivedVideos::addHighResParticipant(const c::MegaChatHandle callId,
                                                 const ParticipantInfo& participant)
{
    if (callId != mCallId)
    {
        std::string msg = "addHighResParticipant: Provided call id (" + str_utils::ch_s(callId) +
                          ") does not match with the expected one (" + str_utils::ch_s(mCallId) +
                          ")";
        clc_log::logMsg(m::logWarning, msg, clc_log::ELogWriter::MEGA_CHAT);
        return c::MegaChatError::ERROR_ARGS;
    }
    if (findHighRes(participant.mClientId))
    {
        return c::MegaChatError::ERROR_EXIST;
    }
    if (mNumberOfHighResVideo != NUM_FOR_INFINITE_VIDEO_RECEIVERS &&
        mHighResParticipants.size() == static_cast<size_t>(mNumberOfHighResVideo))
    {
        return c::MegaChatError::ERROR_TOOMANY;
    }
    mHighResParticipants.push_back(participant);
    return c::MegaChatError::ERROR_OK;
}

int CLCCallReceivedVideos::addLowResParticipant(const c::MegaChatHandle callId,
                                                const ParticipantInfo& participant)
{
    if (callId != mCallId)
    {
        std::string msg = "addLowResParticipant: Provided call id (" + str_utils::ch_s(callId) +
                          ") does not match with the expected one (" + str_utils::ch_s(mCallId) +
                          ")";
        clc_log::logMsg(m::logWarning, msg, clc_log::ELogWriter::MEGA_CHAT);
        return c::MegaChatError::ERROR_ARGS;
    }
    if (findLowRes(participant.mClientId))
    {
        return c::MegaChatError::ERROR_EXIST;
    }
    if (mNumberOfLowResVideo != NUM_FOR_INFINITE_VIDEO_RECEIVERS &&
        mLowResParticipants.size() == static_cast<size_t>(mNumberOfLowResVideo))
    {
        return c::MegaChatError::ERROR_TOOMANY;
    }
    mLowResParticipants.push_back(participant);
    return c::MegaChatError::ERROR_OK;
}

int CLCCallReceivedVideos::updateParticipantHighResVideoState(const c::MegaChatHandle callId,
                                                              const c::MegaChatHandle clientId,
                                                              const bool videoState)
{
    if (callId != mCallId)
    {
        std::string msg = "updateParticipantHighResVideoState: Provided call id (" +
                          str_utils::ch_s(callId) + ") does not match with the expected one (" +
                          str_utils::ch_s(mCallId) + ")";
        clc_log::logMsg(m::logWarning, msg, clc_log::ELogWriter::MEGA_CHAT);
        return c::MegaChatError::ERROR_ARGS;
    }
    if (auto it = findHighRes(clientId); it)
    {
        (*it)->mIsReceivingVideo = videoState;
        return c::MegaChatError::ERROR_OK;
    }
    return c::MegaChatError::ERROR_NOENT;
}

int CLCCallReceivedVideos::updateParticipantLowResVideoState(const c::MegaChatHandle callId,
                                                             const c::MegaChatHandle clientId,
                                                             const bool videoState)
{
    if (callId != mCallId)
    {
        std::string msg = "updateParticipantLowResVideoState: Provided call id (" +
                          str_utils::ch_s(callId) + ") does not match with the expected one (" +
                          str_utils::ch_s(mCallId) + ")";
        clc_log::logMsg(m::logWarning, msg, clc_log::ELogWriter::MEGA_CHAT);
        return c::MegaChatError::ERROR_ARGS;
    }
    if (auto it = findLowRes(clientId); it)
    {
        (*it)->mIsReceivingVideo = videoState;
        return c::MegaChatError::ERROR_OK;
    }
    return c::MegaChatError::ERROR_NOENT;
}

int CLCCallReceivedVideos::removeParticipant(const c::MegaChatHandle callId,
                                             const c::MegaChatHandle clientId)
{
    if (callId != mCallId)
    {
        std::string msg = "removeParticipant: Provided call id (" + str_utils::ch_s(callId) +
                          ") does not match with the expected one (" + str_utils::ch_s(mCallId) +
                          ")";
        clc_log::logMsg(m::logWarning, msg, clc_log::ELogWriter::MEGA_CHAT);
        return c::MegaChatError::ERROR_ARGS;
    }
    bool found = false;
    if (auto itLow = findLowRes(clientId); itLow)
    {
        found = true;
        mLowResParticipants.erase(*itLow);
    }
    if (auto itHigh = findHighRes(clientId); itHigh)
    {
        found = true;
        mHighResParticipants.erase(*itHigh);
    }
    if (!found)
    {
        return c::MegaChatError::ERROR_NOENT;
    }
    return c::MegaChatError::ERROR_OK;
}

void CLCCallReceivedVideos::resetNumberOfLowResVideo(int newNumberOfLow)
{
    mNumberOfLowResVideo = newNumberOfLow;
    mLowResParticipants.clear();
}

void CLCCallReceivedVideos::resetNumberOfHighResVideo(int newNumberOfHigh)
{
    mNumberOfHighResVideo = newNumberOfHigh;
    mHighResParticipants.clear();
}

std::string CLCCallReceivedVideos::receivingVideoReport() const
{
    std::string result = "Call: " + str_utils::ch_s(mCallId) + ".";
    if (mLowResParticipants.size())
    {
        result += " Receiving low res video from " + std::to_string(mLowResParticipants.size()) +
                  " users (" + getParticipantsSummary(mLowResParticipants) + ").";
    }
    if (mHighResParticipants.size())
    {
        result += " Receiving high res video from " + std::to_string(mHighResParticipants.size()) +
                  " users (" + getParticipantsSummary(mHighResParticipants) + ").";
    }
    return result;
}

std::optional<std::vector<ParticipantInfo>::iterator>
    CLCCallReceivedVideos::findLowRes(const c::MegaChatHandle clientId)
{
    auto it = std::find_if(mLowResParticipants.begin(),
                           mLowResParticipants.end(),
                           [clientId](const ParticipantInfo& p)
                           {
                               return p.mClientId == clientId;
                           });
    if (it != mLowResParticipants.end())
    {
        return it;
    }
    return std::nullopt;
}

std::optional<std::vector<ParticipantInfo>::iterator>
    CLCCallReceivedVideos::findHighRes(const c::MegaChatHandle clientId)
{
    auto it = std::find_if(mHighResParticipants.begin(),
                           mHighResParticipants.end(),
                           [clientId](const ParticipantInfo& p)
                           {
                               return p.mClientId == clientId;
                           });
    if (it != mHighResParticipants.end())
    {
        return it;
    }
    return std::nullopt;
}

std::string
    CLCCallReceivedVideos::getParticipantsSummary(const std::vector<ParticipantInfo>& participants)
{
    if (participants.empty())
    {
        return "";
    }
    std::string result = getParticipantRepr(participants[0]);
    for (auto it = participants.begin() + 1; it != participants.end(); ++it)
    {
        result += ", " + getParticipantRepr(*it);
    }
    return result;
}

std::string CLCCallReceivedVideos::getParticipantRepr(const ParticipantInfo& participant)
{
    std::string result = "participant{clientId: " + std::to_string(participant.mClientId);
    result += ", userId: " + str_utils::ch_s(participant.mUserId) + "}";
    return result;
}

}
