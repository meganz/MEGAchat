#include "sdkApi.h"
#include "userAttrCache.h"
#include "chatClient.h"
#include "db.h"
#include <codecvt>
#include <locale>

using namespace promise;
using namespace std;

namespace karere
{
Buffer* ecKeyBase64ToBin(const ::mega::MegaRequest& result)
{
    auto text = result.getText();
    auto len = strlen(text);
    if (len != 43)
        throw std::runtime_error("ecKeyBase64ToBin: Bad EC key len in base64 - must be 43 bytes");
    Buffer* buf = new Buffer(32);
    buf->setDataSize(32);
    base64urldecode(text, len, buf->buf(), 32);
    return buf;
}
const char* nonWhitespaceStr(const char* str)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
    std::u16string u16 = convert.from_bytes(str);
    for (auto s: u16)
    {
        if (!iswblank(s))
            return str;
    }
    return nullptr;
}

UserAttrDesc gUserAttrDescs[8] =
{ //getData func | changeMask
  //0 - avatar
   { [](const ::mega::MegaRequest& req)->Buffer* { return new Buffer(req.getFile(), strlen(req.getFile())); }, ::mega::MegaUser::CHANGE_TYPE_AVATAR},
  //1 - first name
   { [](const ::mega::MegaRequest& req)->Buffer* { return new Buffer(req.getText(), strlen(req.getText())); }, ::mega::MegaUser::CHANGE_TYPE_FIRSTNAME},
  //2 - lastname is handled specially, so we don't use a descriptor for it
   { [](const ::mega::MegaRequest& req)->Buffer* { throw std::runtime_error("Not implementyed"); }, ::mega::MegaUser::CHANGE_TYPE_LASTNAME},
  //3 - authring
  { [](const ::mega::MegaRequest& req)->Buffer* { throw std::runtime_error("Not implementyed"); }, ::mega::MegaUser::CHANGE_TYPE_AUTHRING},
  //4 - last interaction
  { [](const ::mega::MegaRequest& req)->Buffer* { throw std::runtime_error("Not implementyed"); }, ::mega::MegaUser::CHANGE_TYPE_LSTINT},
  //5 - ed25519 signing key
  { [](const ::mega::MegaRequest& req)->Buffer* { return ecKeyBase64ToBin(req); }, ::mega::MegaUser::CHANGE_TYPE_PUBKEY_ED255},
  //6 - cu25519 encryption key
  { [](const ::mega::MegaRequest& req)->Buffer* { return ecKeyBase64ToBin(req); }, ::mega::MegaUser::CHANGE_TYPE_PUBKEY_CU255},
  //7 - keyring - not used by userAttrCache
  { [](const ::mega::MegaRequest& req)->Buffer* { throw std::runtime_error("Not implemented"); }, ::mega::MegaUser::CHANGE_TYPE_KEYRING}
};

UserAttrCache::~UserAttrCache()
{
    mClient.api.sdk.removeGlobalListener(this);
}

void UserAttrCache::dbWrite(UserAttrPair key, const Buffer& data)
{
    sqliteQuery(mClient.db,
        "insert or replace into userattrs(userid, type, data) values(?,?,?)",
        key.user.val, key.attrType, data);
    UACACHE_LOG_DEBUG("dbWrite attr %s", key.toString().c_str());
}

void UserAttrCache::dbWriteNull(UserAttrPair key)
{
    sqliteQuery(mClient.db,
        "insert or replace into userattrs(userid, type, data) values(?,?,NULL)",
        key.user, key.attrType);
    UACACHE_LOG_DEBUG("dbWriteNull attr %s as NULL", key.toString().c_str());
}

UserAttrCache::UserAttrCache(Client& aClient): mClient(aClient)
{
    //load all attributes from db
    SqliteStmt stmt(mClient.db, "select userid, type, data from userattrs");
    while(stmt.step())
    {
        std::unique_ptr<Buffer> data(new Buffer((size_t)sqlite3_column_bytes(stmt, 2)));
        stmt.blobCol(2, *data);
        UserAttrPair key(stmt.uint64Col(0), stmt.intCol(1));
        emplace(std::make_pair(key, std::make_shared<UserAttrCacheItem>(
                *this, data.release(), kCacheFetchNotPending)));
//        UACACHE_LOG_DEBUG("loaded attr %s", key.toString().c_str());
    }
    UACACHE_LOG_DEBUG("loaded %zu entries from db", size());
    mClient.api.sdk.addGlobalListener(this);
}

const char* attrName(uint8_t type)
{
    switch (type)
    {
    case ::mega::MegaApi::USER_ATTR_AVATAR: return "AVATAR";
    case ::mega::MegaApi::USER_ATTR_FIRSTNAME: return "FIRSTNAME";
    case ::mega::MegaApi::USER_ATTR_LASTNAME: return "LASTNAME";
    case ::mega::MegaApi::USER_ATTR_AUTHRING: return "AUTHRING";
    case ::mega::MegaApi::USER_ATTR_LAST_INTERACTION: return "LAST_INTERACTION";
    case ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY: return "PUB_ED25519";
    case ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY: return "PUB_CU25519";
    case ::mega::MegaApi::USER_ATTR_KEYRING: return "KEYRING";
    case USER_ATTR_RSA_PUBKEY: return "PUB_RSA";
    default: return "(invalid)";
    }
}

void UserAttrCache::onUserAttrChange(::mega::MegaUser& user)
{
    int changed = user.getChanges();
//  printf("user %s changed %u\n", Id(user.getHandle()).toString().c_str(), changed);
    for (size_t t = 0; t < sizeof(gUserAttrDescs)/sizeof(gUserAttrDescs[0]); t++)
    {
        if ((changed & gUserAttrDescs[t].changeMask) == 0)
            continue;
        UserAttrPair key(user.getHandle(), t);
        auto it = find(key);
        if (it == end()) //we don't have such attribute
        {
            UACACHE_LOG_DEBUG("Attr %s change received for unknown user, ignoring", attrName(t));
            continue;
        }
        auto& item = it->second;
        dbInvalidateItem(key); //immediately invalidate parsistent cache
        if (item->cbs.empty()) //we aren't using that item atm
        { //delete it from memory as well, forcing it to be freshly fetched if it's requested
            erase(key);
            UACACHE_LOG_DEBUG("Attr %s change received, attr is unused -> deleted from cache",
                key.toString().c_str());
            continue;
        }
        if (item->pending)
        {
            //TODO: Shouldn't we schedule a re-fetch?
            UACACHE_LOG_DEBUG("Attr %s change received, but already fetch in progress, ignoring",
                key.toString().c_str());
            continue;
        }
        UACACHE_LOG_DEBUG("Attr %s change received, invalidated and re-fetching",
            key.toString().c_str());
        item->pending = kCacheFetchUpdatePending;
        fetchAttr(key, item);
    }
}
void UserAttrCache::dbInvalidateItem(UserAttrPair key)
{
    sqliteQuery(mClient.db, "delete from userattrs where userid=? and type=?",
                key.user, key.attrType);
}

void UserAttrCacheItem::notify()
{
    for (auto it=cbs.begin(); it!=cbs.end();)
    {
        auto curr = it;
        it++;
        curr->cb(data.get(), curr->userp); //may erase curr
        if (curr->oneShot)
            cbs.erase(curr);
    }
}

void UserAttrCacheItem::resolve(UserAttrPair key)
{
    pending = kCacheFetchNotPending;
    UACACHE_LOG_DEBUG("Attr %s fetched, writing to db and doing callbacks...", key.toString().c_str());
    parent.dbWrite(key, *data);
    notify();
}
void UserAttrCacheItem::resolveNoDb(UserAttrPair key)
{
    pending = kCacheFetchNotPending;
    UACACHE_LOG_DEBUG("Attr %s fetched but not writing to db, doing callbacks...", key.toString().c_str());
    notify();
}
void UserAttrCacheItem::error(UserAttrPair key, int errCode)
{
    pending = kCacheFetchNotPending;
    data.reset();
    if (errCode == ::mega::API_ENOENT)
    {
        parent.dbWriteNull(key);
        UACACHE_LOG_DEBUG("Attr %s not found on server, clearing from db and doing callbacks...", key.toString().c_str());
    }
    else
    {
        UACACHE_LOG_DEBUG("Attr %s fetch error %d, not touching db and doing callbacks...", key.toString().c_str(), errCode);
    }
    notify();
}

uint64_t UserAttrCache::addCb(iterator itemit, UserAttrReqCbFunc cb, void* userp, bool oneShot)
{
    auto& cbs = itemit->second->cbs;
    auto it = cbs.emplace(cbs.end(), cb, userp, oneShot);
    mCallbacks.emplace(std::piecewise_construct, std::forward_as_tuple(++mCbId),
                       std::forward_as_tuple(itemit, it));
    return mCbId;
}

bool UserAttrCache::removeCb(const uint64_t& cbid)
{
    auto it = mCallbacks.find(cbid);
    if (it == mCallbacks.end())
        return false;
    auto& cbDesc = it->second;
    cbDesc.itemit->second->cbs.erase(cbDesc.cbit);
    return true;
}

uint64_t UserAttrCache::getAttr(const uint64_t& userHandle, unsigned type,
            void* userp, UserAttrReqCbFunc cb, bool oneShot)
{
    UserAttrPair key(userHandle, type);
    auto it = find(key);
    if (it != end())
    {
        auto& item = it->second;
        if (cb)
        { //TODO: not optimal to store each cb pointer, as these pointers would be mostly only a few, with different userp-s
            if (item->pending != kCacheFetchNewPending)
            {
                auto cbid = oneShot ? 0 : addCb(it, cb, userp, false);
                cb(item->data.get(), userp);
                return cbid;
            }
            else
            {
                return addCb(it, cb, userp, oneShot);
            }
        }
        else
        {
            return 0;
        }
    }
    UACACHE_LOG_DEBUG("Attibute %s not found in cache, fetching", key.toString().c_str());
    auto item = std::make_shared<UserAttrCacheItem>(*this, nullptr, kCacheFetchNewPending);
    it = emplace(key, item).first;
    uint64_t cbid = cb ? addCb(it, cb, userp, oneShot) : 0;
    fetchAttr(key, item);
    return cbid;
}

void UserAttrCache::fetchAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    if (!mIsLoggedIn)
        return;
    switch (key.attrType)
    {
        case ::mega::MegaApi::USER_ATTR_LASTNAME:
            fetchUserFullName(key, item);
            break;
        case USER_ATTR_RSA_PUBKEY:
            fetchRsaPubkey(key, item);
            break;
        default:
            fetchStandardAttr(key, item);
            break;
    }
}
void UserAttrCache::fetchStandardAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    mClient.api.call(&::mega::MegaApi::getUserAttribute,
        key.user.toString().c_str(), (int)key.attrType)
    .then([this, key, item](ReqResult result)
    {
        item->data.reset(gUserAttrDescs[key.attrType].getData(*result));
        item->resolve(key);
    })
    .fail([this, key, item](const promise::Error& err)
    {
        item->error(key, err.code());
        return err;
    });
}
void UserAttrCache::fetchUserFullName(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    std::string userid = key.user.toString();
    item->data.reset(new Buffer);

    mClient.api.call(&::mega::MegaApi::getUserAttribute, userid.c_str(),
            (int)::mega::MegaApi::USER_ATTR_FIRSTNAME)
    .then([this, userid, item](ReqResult result)
    {
        //first name. Write a prefix byte with the first name data length,
        //and then the name string in utf8
        auto& data = *(item->data);
        const char* name = nonWhitespaceStr(result->getText());
        if (name)
        {
            size_t len = strlen(name);
            if (len > 255) //FIXME: This is utf8, so can't truncate arbitrarily
            {
                //truncate first name
                data.append<unsigned char>(255);
                data.append(name, 252);
                data.append("...", 3);
            }
            else
            {
                data.append<unsigned char>(len);
                data.append(name);
            }
        }
        else
        {
            data.append<unsigned char>(0);
        }
    })
    .fail([this, item](const promise::Error& err) -> promise::Promise<void>
    {
        if (err.code() != ::mega::API_EARGS)
            return err;
        KR_LOG_DEBUG("No first name for user, proceeding with fetching second name");
        item->data->append<unsigned char>(0);
         //silently ignore errors for the first name, in case we can still retrieve the second name
        return promise::_Void();
    })
    .then([this, userid]()
    {
        return mClient.api.call(&::mega::MegaApi::getUserAttribute, userid.c_str(),
            (int)::mega::MegaApi::USER_ATTR_LASTNAME);
    })
    .then([this, item, key](ReqResult result)
    { //second name
        const char* name = nonWhitespaceStr(result->getText());
        if (name)
        {
            assert(item->data);
            item->data->append(' ');
            item->data->append(name).append<char>(0);
            item->resolve(key);
        }
        else //second name is NULL
        {
            if (item->data->dataSize() > 1)
                item->resolve(key);
            else
                item->error(key, ::mega::API_ENOENT);
        }
    })
    .fail([this, key, item](const promise::Error& err)
    {
//even if we have error here, we don't clear item->data as we may have the
//first name, but won't cache it in db, so the next app run will retry
        if (err.code() == ::mega::API_ENOENT)
        {
            if (item->data) //has only one name, still good
                item->resolve(key);
            else
                item->error(key, ::mega::API_ENOENT);
        }
        else //some other error
        {
            if (item->data)
                item->resolveNoDb(key);
            else
                item->error(key, err.code());
        }
    });
}
void UserAttrCache::fetchRsaPubkey(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    mClient.api.call(&::mega::MegaApi::getUserData, key.user.toString().c_str())
    .fail([this, key, item](const promise::Error& err)
    {
        item->error(key, err.code());
        return err;
    })
    .then([this, key, item](ReqResult result) -> promise::Promise<void>
    {
        auto rsakey = result->getPassword();
        size_t keylen;
        if (!rsakey || ((keylen = strlen(rsakey)) < 1))
        {
            KR_LOG_WARNING("Public RSA key returned by API for user %s is null or empty", key.user.toString().c_str());
            item->error(key, ::mega::API_ENOENT);
            return promise::Error("No key", ::mega::API_ENOENT, ERRTYPE_MEGASDK);
        }
        item->data.reset(new Buffer(keylen+1));
        int binlen = base64urldecode(rsakey, keylen, item->data->buf(), keylen);
        item->data->setDataSize(binlen);
        item->resolve(key);
        return promise::_Void();
    });
}

void UserAttrCache::onLogin()
{
    mIsLoggedIn = true;
    for (auto& item: *this)
    {
        if (item.second->pending != kCacheFetchNotPending)
            fetchAttr(item.first, item.second);
    }
}

promise::Promise<Buffer*>
UserAttrCache::getAttr(const uint64_t &user, unsigned attrType)
{
    auto pms = new Promise<Buffer*>;
    auto ret = *pms;
    getAttr(user, attrType, pms, [](Buffer* buf, void* userp)
    {
        auto p = reinterpret_cast<Promise<Buffer*>*>(userp);
        if (buf)
            p->resolve(buf);
        else
            p->reject("User attribute fetch failed");
        delete p;
    }, true);
    return ret;
}

}
