#ifndef CHATCOMMON_H
#define CHATCOMMON_H
#include <murmurHash/MurmurHash3.h>

namespace karere
{

static inline std::string userIdFromJid(const std::string& jid)
{
    size_t pos = jid.find('@');
    if((pos == 0) || (pos == std::string::npos) || (pos == jid.size()-1))
        throw std::runtime_error("userIdFromJid: invalid JID: "+jid);
    return jid.substr(0, pos);
}

}

#endif // CHATCOMMON_H
