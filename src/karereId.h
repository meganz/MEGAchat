#ifndef _ID_H_INCLUDED_
#define _ID_H_INCLUDED_

#include <stdint.h>
#include <string>
#include <set>
#include "base64url.h"
#include <buffer.h>

namespace karere
{
class Id
{
public:
    uint64_t val;
    std::string toString() const { return base64urlencode(&val, sizeof(val)); }
    bool isValid() const { return val != ~((uint64_t)0); }
    Id(const uint64_t& from=0): val(from){}
    explicit Id(const char* b64, size_t len=0) { base64urldecode(b64, len?len:strlen(b64), &val, sizeof(val)); }
    bool operator==(const Id& other) const { return val == other.val; }
    bool operator==(const uint64_t& aVal) const { return val == aVal; }
    Id& operator=(const Id& other) { val = other.val; return *this; }
    Id& operator=(const uint64_t& aVal) { val = aVal; return *this; }
    operator const uint64_t&() const { return val; }
    bool operator<(const Id& other) const { return val < other.val; }
    static const Id null() { return static_cast<uint64_t>(0); }
    static const Id inval() { return ~((uint64_t)0); }
    static const Id COMMANDER() { return Id("gTxFhlOd_LQ"); }
    /** Comparison at byte level, necessary to compatibility with the webClient (javascript)*/
    static bool greaterThanForJs(const Id &first, const Id &second)
    {
        return (memcmp(&(first.val), &(second.val), sizeof(uint64_t)) > 0);
    }
};


//for exception message purposes
static inline std::string operator+(const char* str, const Id& id)
{
    std::string result(str);
    result.append(id.toString());
    return result;
}
static inline std::string& operator+(std::string&& str, const Id& id)
{
    str.append(id.toString());
    return str;
}

struct SetOfIds: public std::set<karere::Id>
{
    typedef std::set<karere::Id> Base;
    template <class T>
    SetOfIds(const T& src) { load(src); }
    SetOfIds(){}
    SetOfIds(Base&& other): Base(std::move(other)){}
    void save(Buffer& buf)
    {
        for (auto id: *this)
            buf.append(id.val);
    }
    void load(const Buffer& buf)
    {
        assert(buf.dataSize() % 8 == 0);
        clear();
        const char* last = buf.buf() + buf.dataSize();
        for (const char* pos = buf.buf(); pos < last; pos += sizeof(uint64_t))
        {
            emplace(Buffer::alignSafeRead<uint64_t>(pos));
        }
    }
    bool has(Id id) { return find(id) != end(); }
};
}

namespace std
{
    template<>
    struct hash<karere::Id> { size_t operator()(const karere::Id& id) const { return hash<uint64_t>()(id.val); } };
}

#endif
