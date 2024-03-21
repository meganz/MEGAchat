#ifndef USERATTRCACHE
#define USERATTRCACHE
#include "base/logger.h"
#include "karereId.h"
#include <megaapi.h>
#include <list>
#include "base/promise.h"
#include <base/trackDelete.h>

#define UACACHE_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_uacache, fmtString, ##__VA_ARGS__)
#define UACACHE_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_uacache, fmtString, ##__VA_ARGS__)

class Buffer;

namespace mega
{
 class MegaRequest;
}
namespace karere
{
/** @brief Virtual user attribute types - these arent' supported directly
 * \c by MegaApi::getUserAttribute()
 */
enum {
    /** First attribute that is not directly supported by the SDK via getUserAttribute()*/
    USER_ATTR_VIRTUAL_FIRST = 64,

    /** USER_ATTR_RSA_PUBKEY = USER_ATTR_VIRTUAL_FIRST, deprecated (used for legacy encryption)*/

    /** The email of the user, as returned bh getUserEmail() */
    USER_ATTR_EMAIL,

    /** The most significant bit in the attribute type is 1, then the attribute is
     * not directly backed by the db, but rather synthesized by other attributes
     */
    USER_ATTR_FLAG_COMPOSITE = 128,
    /** Returns firstname and secondname in one string. Tolerates either name
     * missing. If both are present, they are separated with a space.
     * Fetches both names using the cache, and does it in parallel. Does not
     * cache the full name itself, but relies on each name being cached separately
     */
    USER_ATTR_FULLNAME = USER_ATTR_FLAG_COMPOSITE | 1
};

const char* attrName(uint8_t type);

class Client;
struct UserAttrDesc
{
    typedef Buffer*(*GetDataFunc)(const ::mega::MegaRequest&);
    GetDataFunc getData;
    uint64_t changeMask;
    UserAttrDesc(GetDataFunc aGetData, uint64_t aChangeMask):
        getData(aGetData), changeMask(aChangeMask){}
};

// Maps uhBin to UserAttrDesc struct
typedef std::map <int, UserAttrDesc> UserAttrDescMap;
extern UserAttrDescMap gUserAttrDescsMap;

struct UserAttrPair
{
    Id user;
    uint8_t attrType;
    Id mPh; // only valid in preview mode to retrieve user-attributes
    bool operator<(const UserAttrPair& other) const
    {
        if (user == other.user)
            return attrType < other.attrType;
        else
            return user < other.user;
    }
    UserAttrPair(uint64_t aUser, uint8_t aType, uint64_t ph = Id::inval()): user(aUser), attrType(aType), mPh(ph)
    {
        if (gUserAttrDescsMap.find(attrType) == gUserAttrDescsMap.end())
        {
            throw std::runtime_error("UserAttrPair: Invalid user attribute id specified");
        }
    }
    std::string toString()
    {
        std::string result;
        result.reserve(32);
        result.append(attrName(attrType)).append(" of user ").append(user.toString())+='(';
        result.append(std::to_string((int64_t)user.val))+=')';
        return result;
    }
};

typedef void(*UserAttrReqCbFunc)(Buffer*, void*);
struct UserAttrCacheItem;

struct UserAttrReqCb: public karere::WeakReferenceable<UserAttrReqCb>
{
    UserAttrCacheItem& owner;
    UserAttrReqCbFunc cb;
    void* userp;
    bool oneShot;
    std::list<UserAttrReqCb>::iterator listIt;
    UserAttrReqCb(UserAttrCacheItem& aOwner, UserAttrReqCbFunc aCb, void* aUserp, bool aOneShot=false)
    : WeakReferenceable(this), owner(aOwner), cb(aCb), userp(aUserp), oneShot(aOneShot){}
};

enum { kCacheFetchNotPending=0, kCacheFetchUpdatePending=1, kCacheFetchNewPending=2, kCacheNotFetchUntilUse=3};

class UserAttrCache;
struct UserAttrCacheItem
{
    UserAttrCache& parent;
    std::unique_ptr<Buffer> data;
    std::list<UserAttrReqCb> cbs;
    unsigned char pending;
    UserAttrCacheItem(UserAttrCache& aParent ,Buffer* buf, unsigned char aPending)
        : parent(aParent), data(buf), pending(aPending){}
    UserAttrReqCb::WeakRefHandle addCb(UserAttrReqCbFunc cb, void* userp, bool oneShot=false);
    void resolve(UserAttrPair key);
    void resolveNoDb(UserAttrPair key); //same as resolve, but dont't write to cache db - used for partial results, like first name obtained, second name returned non-ENOENT error
    void error(UserAttrPair key, int errCode);
    void errorNoDb(int errCode);
    void notify();
};
/** @brief
 * User attribute cache, prividing notifications when an attribute is changed
 */
class UserAttrCache: public std::map<UserAttrPair, std::shared_ptr<UserAttrCacheItem>>,
                     public ::mega::MegaGlobalListener, public karere::DeleteTrackable
{
protected:
    Client& mClient;
    bool mIsLoggedIn = false;
    void dbWrite(UserAttrPair key, const Buffer& data);
    void dbWriteNull(UserAttrPair key);
    void dbInvalidateItem(UserAttrPair item);
    void fetchAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);

//actual attrib fetch backend functions
    void fetchUserFullName(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
    void fetchStandardAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
    void fetchEmail(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item);
//==
    void onUserAttrChange(uint64_t userid, uint64_t changed);
    void onUserAttrChange(::mega::MegaUser& user);
    void onLogin();
    /** @brief Invalidates the whole cache, and re-fetches all registered queries.
     * Used when we discover that karere state is out of sync with SDK state
     */
    void invalidate();
    void onLogOut();
    friend struct UserAttrCacheItem;
    friend class Client;
    friend class ContactList;
public:
    /** @brief The cache request handle, that identifies a specific cache request
     * and update monitoring callback. This handle can be used to cancel the
     * attribute request and/or subsequent updates. This is essentially a weak pointer
     * to the internal representation of the request. As such, the following methods
     * can be used on it:
     * \c isValid() to check if the handle actually references a valid request. It will
     * return false if it's a handle to a one-shot request that has executed, or
     * if it has not been assigned a valid value, as returned by \c getAttr()
     */
    typedef UserAttrReqCb::WeakRefHandle Handle;
    UserAttrCache(Client& aClient);
    ~UserAttrCache();
    /** @brief gets the attribute \c attrType of user \c user. When the attribute
     * is successfully obtained, the callback \c will be called with a Buffer object, containing
     * the attribute data. If there is an error obraining the attribute, the callback
     * will be called with a \c null Buffer object.
     * @param user User from attribute is request
     * @param attrType Attribute type
     * @param userp An arbitrary user-supplied pointer that will be passed to the
     * callback
     * @param cb Callback called when attribute is available
     * @param oneShot If \true, the callback will be called only once, and will
     * @param fetch If attribute doesn't exist, the callback is resgitered but the attribute is not fetched
     * @param ph Public handle for previews
     * be unregistered immediately after that. If \false, the callback will be called
     * every time the attribute changes on the server and the new value is fetched.
     */
    Handle getAttr(uint64_t user, unsigned attrType, void* userp,
                             UserAttrReqCbFunc cb, bool oneShot=false, bool fetch = true, uint64_t ph = Id::inval());
    /** @brief A promise-based version of \c getAttr. The request
     * is implicitly one-shot, as a promise can be resolved only once.
     */
    promise::Promise<Buffer*> getAttr(uint64_t user, unsigned attrType, uint64_t ph = Id::inval());
    /** @brief Unregisters an attribute request/subsequent callbacks.
     * It can be a not-yet-fetched single shot request as well. Use this method
     * to unsubscribe from further calling the corresponding callback.
     * @returns \c true if the request existed, or \c false if no such
     * request is currently registered (expired one-shot for example).
     */
    bool removeCb(Handle handle);

    promise::Promise<void> getAttributes(uint64_t user, uint64_t ph = Id::inval());

    const Buffer *getDataFromCache(uint64_t user, unsigned attrType);
};

}
#endif // USERATTRCACHE

