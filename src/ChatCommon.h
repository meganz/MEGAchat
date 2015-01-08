#ifndef CHATCOMMON_H
#define CHATCOMMON_H
#include <murmurHash/MurmurHash3.h>

#define KARERE_XMPP_DOMAIN "developers.mega.co.nz"

namespace karere
{

static inline std::string userIdFromJid(const std::string& jid)
{
    size_t pos = jid.find('@');
    if((pos == 0) || (pos == std::string::npos) || (pos == jid.size()-1))
        throw std::runtime_error("userIdFromJid: invalid JID: "+jid);
    return jid.substr(0, pos);
}

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
}

#endif // CHATCOMMON_H
