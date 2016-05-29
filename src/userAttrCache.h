#ifndef USERATTRCACHE
#define USERATTRCACHE

#include "karereId.h"
#include <megaapi.h>

class Buffer;

namespace mega
{
 class MegaRequest;
}
namespace karere
{
enum { USER_ATTR_RSA_PUBKEY = 128 }; //virtual user attribute type, to be used with the common attr cache table

class Client;
struct UserAttrDesc
{
    Buffer*(*getData)(const mega::MegaRequest&);
    int changeMask;
};

extern UserAttrDesc gUserAttrDescs[8];

struct UserAttrPair
{
    Id user;
    unsigned attrType;
    bool operator<(const UserAttrPair& other) const
    {
        if (user == other.user)
            return attrType < other.attrType;
        else
            return user < other.user;
    }
    UserAttrPair(uint64_t aUser, unsigned aType): user(aUser), attrType(aType)
    {
        if (attrType >= sizeof(gUserAttrDescs)/sizeof(gUserAttrDescs[0]))
            throw std::runtime_error("UserAttrPair: Invalid user attribute id specified");
    }
};
typedef void(*UserAttrReqCbFunc)(Buffer*, void*);
struct UserAttrReqCb
{
    UserAttrReqCbFunc cb;
    void* userp;
    UserAttrReqCb(UserAttrReqCbFunc aCb, void* aUserp): cb(aCb), userp(aUserp){}
};

enum { kCacheFetchNotPending=0, kCacheFetchUpdatePending=1, kCacheFetchNewPending=2};

class UserAttrCache;
struct UserAttrCacheItem
{
    UserAttrCache& parent;
    std::unique_ptr<Buffer> data;
    std::list<UserAttrReqCb> cbs;
    unsigned char pending;
    UserAttrCacheItem(UserAttrCache& aParent ,Buffer* buf, bool aPending)
        : parent(aParent), data(buf), pending(aPending){}
    void resolve(UserAttrPair key);
    void resolveNoDb(UserAttrPair key); //same as resolve, but dont't write to cache db - used for partial results, like first name obtained, second name returned non-ENOENT error
    void error(UserAttrPair key, int errCode);
    void notify();
};

class UserAttrCache: public std::map<UserAttrPair, std::shared_ptr<UserAttrCacheItem>>,
                     public mega::MegaGlobalListener
{
protected:
    struct CbRefItem
    {
        iterator itemit;
        std::list<UserAttrReqCb>::iterator cbit;
        CbRefItem(iterator aItemIt, std::list<UserAttrReqCb>::iterator aCbIt)
            :itemit(aItemIt), cbit(aCbIt){}
    };
    Client& mClient;
    uint64_t mCbId = 0;
    std::map<uint64_t, CbRefItem> mCallbacks;
    void dbWrite(UserAttrPair key, const Buffer& data);
    void dbWriteNull(UserAttrPair key);
    void dbInvalidateItem(UserAttrPair item);
    uint64_t addCb(iterator itemit, UserAttrReqCbFunc cb, void* userp);
    void fetchAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
//actual attrib fetch backend functions
    void fetchUserFullName(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
    void fetchStandardAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
    void fetchRsaPubkey(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
//==
    void onUserAttrChange(mega::MegaUser& user);
    void onLogin();
    friend struct UserAttrCacheItem;
    friend class Client;
public:
    UserAttrCache(Client& aClient);
    ~UserAttrCache();
    uint64_t getAttr(const uint64_t& user, unsigned attrType, void* userp,
                             UserAttrReqCbFunc cb);
    promise::Promise<Buffer*> getAttr(const uint64_t &user, unsigned attrType);
    bool removeCb(const uint64_t &cbid);
};

}
#endif // USERATTRCACHE

