#ifndef MCLC_REPORTS_H
#define MCLC_REPORTS_H

/**
 * @file
 * @brief This file contains some useful functions to make reports, e.g. a summary of a public chat,
 * process and organize a set of chat messages, etc.
 */

#include <map>
#include <megachatapi.h>
#include <optional>
namespace c = ::megachat;

namespace mclc::clc_report
{
static const int MAX_NUMBER_MESSAGES = 100; // chatd doesn't allow more than 256

/**
 * @brief Writes to a file the contents of the chat with the given chat handle. The file is the one
 * specified in the g_reviewPublicChatOutFile global variable.
 */
void reviewPublicChatLoadMessages(const c::MegaChatHandle chatid);

/**
 * @brief Extracts and organize relevant information inside msg and prints it to the cout and also
 * to the g_reviewPublicChatOutFile file.
 *
 * @param chatid The chat to extract the messages from
 * @param msg The messages
 * @param loadorreceive A text with information about the reception of the message.
 */
void reportMessageHuman(c::MegaChatHandle chatid,
                        c::MegaChatMessage* msg,
                        const char* loadorreceive);
void reportMessage(c::MegaChatHandle chatid, c::MegaChatMessage* msg, const char* loadorreceive);

struct ParticipantInfo
{
    c::MegaChatHandle mClientId;
    c::MegaChatHandle mUserId;
    bool mIsReceivingVideo;
};

class CLCCallReceivedVideos
{
public:
    static constexpr int NUM_FOR_INFINITE_VIDEO_RECEIVERS = -1;
    void resetNumberOfLowResVideo(int newNumberOfLow);
    void resetNumberOfHighResVideo(int newNumberOfHigh);

    int addHighResParticipant(const c::MegaChatHandle callId, const ParticipantInfo& participant);
    int addLowResParticipant(const c::MegaChatHandle callId, const ParticipantInfo& participant);

    int updateParticipantHighResVideoState(const c::MegaChatHandle callId,
                                           const c::MegaChatHandle clientId,
                                           const bool videoState);
    int updateParticipantLowResVideoState(const c::MegaChatHandle callId,
                                          const c::MegaChatHandle clientId,
                                          const bool videoState);

    int removeParticipant(const c::MegaChatHandle callId, const c::MegaChatHandle clientId);

    void setCallId(const c::MegaChatHandle callId)
    {
        mCallId = callId;
    }

    std::string receivingVideoReport() const;

    bool isValid() const
    {
        return mCallId != c::MEGACHAT_INVALID_HANDLE;
    }

private:
    c::MegaChatHandle mCallId{c::MEGACHAT_INVALID_HANDLE};
    int mNumberOfLowResVideo{0};
    int mNumberOfHighResVideo{0};
    std::vector<ParticipantInfo> mLowResParticipants;
    std::vector<ParticipantInfo> mHighResParticipants;

    std::optional<std::vector<ParticipantInfo>::iterator>
        findLowRes(const c::MegaChatHandle clientId);
    std::optional<std::vector<ParticipantInfo>::iterator>
        findHighRes(const c::MegaChatHandle clientId);

    static std::string getParticipantsSummary(const std::vector<ParticipantInfo>& participants);
    static std::string getParticipantRepr(const ParticipantInfo& participant);
};
}
#endif // MCLC_REPORTS_H
