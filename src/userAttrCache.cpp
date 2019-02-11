#include "sdkApi.h"
#include "userAttrCache.h"
#include "chatClient.h"
#include "db.h"
#ifndef _MSC_VER
#include <codecvt> // deprecated
#endif
#include <locale>
#include <mega/types.h>

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
#ifndef _MSC_VER
    // codecvt deprecated
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
    std::u16string u16 = convert.from_bytes(str);
#else
    std::string result16;
    ::mega::MegaApi::utf8ToUtf16(str, &result16);
    std::u16string u16((char16_t*)result16.data(), result16.size() / 2);
#endif

    for (auto s: u16)
    {
        if (!iswblank(s))
            return str;
    }
    return nullptr;
}

inline static Buffer* bufFromCstr(const char* cstr)
{
    return new Buffer(cstr, strlen(cstr));
}

inline static Buffer* bufFromTLV(const ::mega::MegaStringMap *map, const char *key)
{
    const char *buf = map->get(key);
    return buf ? new Buffer(buf, strlen(buf)) : new Buffer(nullptr, 0);
}

Buffer* getDataNotImpl(const ::mega::MegaRequest& /*req*/)
{
     throw std::runtime_error("Not implemented");
}

UserAttrDesc gUserAttrDescs[21] =
{ //attrib code, getData func, changeMask
  //avatar
    {
      ::mega::MegaApi::USER_ATTR_AVATAR,
      [](const ::mega::MegaRequest& req)->Buffer* { return bufFromCstr(req.getFile()); },
      ::mega::MegaUser::CHANGE_TYPE_AVATAR
    },
  //first name
    {
      ::mega::MegaApi::USER_ATTR_FIRSTNAME,
      [](const ::mega::MegaRequest& req)->Buffer* { return bufFromCstr(req.getText()); },
      ::mega::MegaUser::CHANGE_TYPE_FIRSTNAME
    },
  //last name
    {
      ::mega::MegaApi::USER_ATTR_LASTNAME,
      [](const ::mega::MegaRequest& req)->Buffer* { return bufFromCstr(req.getText()); },
      ::mega::MegaUser::CHANGE_TYPE_LASTNAME
    },
  //authring
    {
      ::mega::MegaApi::USER_ATTR_AUTHRING,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_AUTHRING
    },
  //last interaction
    {
      ::mega::MegaApi::USER_ATTR_LAST_INTERACTION,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_LSTINT
    },
  //ed25519 signing key
    {
      ::mega::MegaApi::USER_ATTR_ED25519_PUBLIC_KEY,
      [](const ::mega::MegaRequest& req)->Buffer* { return ecKeyBase64ToBin(req); },
      ::mega::MegaUser::CHANGE_TYPE_PUBKEY_ED255
    },
  //cu25519 encryption key
    {
      ::mega::MegaApi::USER_ATTR_CU25519_PUBLIC_KEY,
      [](const ::mega::MegaRequest& req)->Buffer* { return ecKeyBase64ToBin(req); },
      ::mega::MegaUser::CHANGE_TYPE_PUBKEY_CU255
    },
  //keyring - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_KEYRING,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_KEYRING
    },

  //RSA-pubk - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_SIG_RSA_PUBLIC_KEY,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_SIG_PUBKEY_RSA
    },
  //CU255 - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_SIG_CU255_PUBLIC_KEY,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_SIG_PUBKEY_CU255
    },
  //Country - not used by userAttrCache
    {
      10,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_COUNTRY
    },
  //BirthDay - not used by userAttrCache
    {
      11,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_BIRTHDAY
    },
  //BirthMonth - not used by userAttrCache
    {
      12,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_BIRTHDAY
    },
  //BirthYear - not used by userAttrCache
    {
      13,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_BIRTHDAY
    },
  //language - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_LANGUAGE,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_LANGUAGE
    },
  //PWD reminder - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_PWD_REMINDER,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_PWD_REMINDER
    },
  //Disable versions - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_DISABLE_VERSIONS,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_DISABLE_VERSIONS
    },
  //Contact link verification - not used by userAttrCache
    {
      ::mega::MegaApi::USER_ATTR_CONTACT_LINK_VERIFICATION,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_CONTACT_LINK_VERIFICATION
    },
  //richLink
    {
      ::mega::MegaApi::USER_ATTR_RICH_PREVIEWS,
      [](const ::mega::MegaRequest& req)->Buffer* { return bufFromTLV(req.getMegaStringMap(), "num"); },
      ::mega::MegaUser::CHANGE_TYPE_RICH_PREVIEWS
    },
  //email
    {
      USER_ATTR_EMAIL,
      &getDataNotImpl, ::mega::MegaUser::CHANGE_TYPE_EMAIL
    },
  //FULLNAME - virtual attrib with no DB backing
    {
      USER_ATTR_FULLNAME,
      &getDataNotImpl,
      ::mega::MegaUser::CHANGE_TYPE_FIRSTNAME | ::mega::MegaUser::CHANGE_TYPE_LASTNAME
    },

};

UserAttrCache::~UserAttrCache()
{
    mClient.api.sdk.removeGlobalListener(this);
}

void UserAttrCache::dbWrite(UserAttrPair key, const Buffer& data)
{
    mClient.db.query(
        "insert or replace into userattrs(userid, type, data) values(?,?,?)",
        key.user.val, key.attrType, data);
    UACACHE_LOG_DEBUG("dbWrite attr %s", key.toString().c_str());
}

void UserAttrCache::dbWriteNull(UserAttrPair key)
{
    mClient.db.query(
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
    case ::mega::MegaApi::USER_ATTR_RICH_PREVIEWS: return "RICH_LINKS";
    case USER_ATTR_EMAIL: return "EMAIL";
    case USER_ATTR_RSA_PUBKEY: return "PUB_RSA";
    case USER_ATTR_FULLNAME: return "FULLNAME";
    default: return "(invalid)";
    }
}
void UserAttrCache::onUserAttrChange(::mega::MegaUser& user)
{
    onUserAttrChange(user.getHandle(), user.getChanges());
}
void UserAttrCache::onUserAttrChange(uint64_t userid, int changed)
{
//  printf("user %s changed %u\n", Id(user.getHandle()).toString().c_str(), changed);
    for (size_t i = 0; i < sizeof(gUserAttrDescs)/sizeof(gUserAttrDescs[0]); i++)
    {
        auto& desc = gUserAttrDescs[i];
        if ((changed & desc.changeMask) == 0)
            continue; //the change is not of this attrib type
        int type = desc.type;
        UserAttrPair key(userid, type);
        auto it = find(key);
        if (it == end()) //we don't have such attribute
        {
            UACACHE_LOG_DEBUG("Attr %s change received for unknown user, ignoring", attrName(type));
            continue;
        }
        auto& item = it->second;
        if ((type & USER_ATTR_FLAG_COMPOSITE) == 0)
        {
            dbInvalidateItem(key); //immediately invalidate persistent cache
        }
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
    mClient.db.query("delete from userattrs where userid=? and type=?",
                key.user, key.attrType);
}

void UserAttrCacheItem::notify()
{
    for (auto it=cbs.begin(); it!=cbs.end();)
    {
        auto curr = it;
        ++it; //curr may be deleted in the callback
        if (curr->oneShot)
        {
            auto wref = curr->weakHandle();
            curr->cb(data.get(), curr->userp); //may erase curr
            parent.removeCb(wref); //checks if already deleted
        }
        else
        {
            curr->cb(data.get(), curr->userp);
        }
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

void UserAttrCacheItem::errorNoDb(int /*errCode*/)
{
    pending = kCacheFetchNotPending;
    data.reset();
    notify();
}

UserAttrCache::Handle UserAttrCacheItem::addCb(UserAttrReqCbFunc cb, void* userp, bool oneShot)
{
    auto it = cbs.emplace(cbs.end(), *this, cb, userp, oneShot);
    it->listIt = it;
    return it->weakHandle();
}

bool UserAttrCache::removeCb(Handle h)
{
    if (!h.isValid())
        return false;
    h->owner.cbs.erase(h->listIt);
    return true;
}

UserAttrCache::Handle UserAttrCache::getAttr(uint64_t userHandle, unsigned type,
            void* userp, UserAttrReqCbFunc cb, bool oneShot)
{
    UserAttrPair key(userHandle, type);
    auto it = find(key);
    if (it != end())
    {
        auto& item = *it->second;
        if (cb)
        { // Maybe not optimal to store each cb pointer, as these pointers would be mostly only a few, with different userp-s
            if (item.pending != kCacheFetchNewPending)
            {
                // we have something in the cache, call the cb
                auto handle = oneShot ? Handle::invalid() : item.addCb(cb, userp, false);
                cb(item.data.get(), userp);
                return handle;
            }
            else //nothing in cache, must always add a callback, even if one shot
            {
                return item.addCb(cb, userp, oneShot);
            }
        }
        else
        {
            // no callback, user wants to force pre-fetching of attribute, but we are
            // already subscribed to it
            return Handle::invalid();
        }
    }

    //we don't have the attrib item, create it
    UACACHE_LOG_DEBUG("Attibute %s not found in cache, fetching", key.toString().c_str());
    auto item = std::make_shared<UserAttrCacheItem>(*this, nullptr, kCacheFetchNewPending);
    it = emplace(key, item).first;
    Handle handle = cb ? item->addCb(cb, userp, oneShot) : Handle::invalid();
    fetchAttr(key, item);
    return handle;
}

void UserAttrCache::fetchAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    if (!mIsLoggedIn && !(key.attrType & USER_ATTR_FLAG_COMPOSITE))
        return;
    switch (key.attrType)
    {
        case USER_ATTR_FULLNAME:
            fetchUserFullName(key, item);
            break;
        case USER_ATTR_RSA_PUBKEY:
            fetchRsaPubkey(key, item);
            break;
        case USER_ATTR_EMAIL:
            fetchEmail(key, item);
            break;
        default:
            fetchStandardAttr(key, item);
            break;
    }
}
void UserAttrCache::fetchStandardAttr(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    auto wptr = weakHandle();
    mClient.api.call(&::mega::MegaApi::getUserAttribute,
        key.user.toString().c_str(), (int)key.attrType)
    .then([wptr, this, key, item](ReqResult result)
    {
        wptr.throwIfDeleted();
        item->data.reset(gUserAttrDescs[key.attrType].getData(*result));
        item->resolve(key);
    })
    .fail([wptr, this, key, item](const ::promise::Error& err)
    {
        wptr.throwIfDeleted();
        item->error(key, err.code());
        return err;
    });
}

void UserAttrCache::fetchEmail(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    auto wptr = weakHandle();
    mClient.api.call(&::mega::MegaApi::getUserEmail,
        key.user.val)
    .then([wptr, this, key, item](ReqResult result)
    {
        wptr.throwIfDeleted();
        auto email = result->getEmail();
        item->data.reset(new Buffer(email, strlen(email)));
        item->resolve(key);
    })
    .fail([wptr, this, key, item](const ::promise::Error& err)
    {
        wptr.throwIfDeleted();
        item->error(key, err.code());
        return err;
    });
}

void UserAttrCache::fetchUserFullName(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    struct Context
    {
        std::string firstname;
        std::string lastname;
    };
    auto ctx = std::make_shared<Context>();
    auto wptr = weakHandle();
    auto pms1 = getAttr(key.user, ::mega::MegaApi::USER_ATTR_FIRSTNAME)
    .then([ctx](Buffer* data)
    {
        if (!data->empty())
            ctx->firstname.assign(data->buf(), data->dataSize());
    })
    .fail([](const Error& /*err*/)
    {
        return _Void();
    });

    auto pms2 = getAttr(key.user, ::mega::MegaApi::USER_ATTR_LASTNAME)
    .then([ctx](Buffer* data)
    {
        if (!data->empty())
            ctx->lastname.assign(data->buf(), data->dataSize());
    })
    .fail([](const Error& /*err*/)
    {
        return _Void();
    });

    ::promise::when(pms1, pms2)
    .then([wptr, this, ctx, key, item]()
    {
        wptr.throwIfDeleted();
        item->data.reset(new Buffer(ctx->firstname.size()+ctx->lastname.size()+1));
        auto& data = *item->data;
        auto& fn = ctx->firstname;
        if (fn.size() > 255)
        {
            fn.resize(252);
            fn.append("...");
        }
        data.append<uint8_t>(fn.size());
        if (!fn.empty())
        {
            data.append(fn);
        }
        if (!ctx->lastname.empty())
        {
            if (!fn.empty())
                data.append<char>(' ');
            data.append(ctx->lastname);
        }
        item->resolveNoDb(key);
    })
    .fail([wptr, item](const Error& err)
    {
        wptr.throwIfDeleted();
        item->errorNoDb(err.code());
        return err;
    });
}

void UserAttrCache::fetchRsaPubkey(UserAttrPair key, std::shared_ptr<UserAttrCacheItem>& item)
{
    auto wptr = weakHandle();
    mClient.api.call(&::mega::MegaApi::getUserData, key.user.toString().c_str())
    .fail([wptr, this, key, item](const ::promise::Error& err)
    {
        wptr.throwIfDeleted();
        item->error(key, err.code());
        return err;
    })
    .then([wptr, this, key, item](ReqResult result) -> ::promise::Promise<void>
    {
        wptr.throwIfDeleted();
        auto rsakey = result->getPassword();
        size_t keylen;
        if (!rsakey || ((keylen = strlen(rsakey)) < 1))
        {
            KR_LOG_WARNING("Public RSA key returned by API for user %s is null or empty", key.user.toString().c_str());
            item->error(key, ::mega::API_ENOENT);
            return ::promise::Error("No key", ::mega::API_ENOENT, ERRTYPE_MEGASDK);
        }
        item->data.reset(new Buffer(keylen+1));
        int binlen = base64urldecode(rsakey, keylen, item->data->buf(), keylen);
        item->data->setDataSize(binlen);
        item->resolve(key);
        return ::promise::_Void();
    });
}

void UserAttrCache::invalidate()
{
    mClient.db.query("delete from userattrs");
    for (auto& item: *this)
    {
        item.second->pending = kCacheFetchUpdatePending;
    }
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

void UserAttrCache::onLogOut()
{
    mIsLoggedIn = false;
}

::promise::Promise<Buffer*>
UserAttrCache::getAttr(uint64_t user, unsigned attrType)
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
