#ifndef CHATCOMMON_H
#define CHATCOMMON_H
#include <murmurHash/MurmurHash3.h>
#include <string>
#include "karereCommon.h" //for std::to_string on android

namespace karere
{

static inline uint32_t fastHash(const char* str, size_t len)
{
    uint32_t result;
    MurmurHash3_x86_32(str, len, 0x4ef5391a, &result);
    return result;
}

static inline uint32_t fastHash(const std::string& str)
{
    return fastHash(str.c_str(), str.size());
}

/**
* Generate a message ID, based on the target JID.
* @returns {string}.
*/
inline std::string generateMessageId(const std::string& jid, const std::string& MessageContents = "")
{
    std::string messageIdHash = jid;
    if (!MessageContents.empty()) {
        messageIdHash += MessageContents;
    }

    return "m" + std::to_string(fastHash(messageIdHash)) + "_" + std::to_string(time(nullptr));
}
}

#endif // CHATCOMMON_H
