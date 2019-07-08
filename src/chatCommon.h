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

}

#endif // CHATCOMMON_H
