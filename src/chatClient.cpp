//we need the POSIX version of strerror_r, not the GNU one
#ifdef _GNU_SOURCE
    #undef _GNU_SOURCE
    #define _POSIX_C_SOURCE 201512L
#endif
#include <string.h>

#include "chatClient.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <direct.h>
    #define mkdir(dir, mode) _mkdir(dir)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtcModule/webrtc.h"
#include "rtcCrypto.h"
#include "dummyCrypto.h" //for makeRandomString
#include "base/services.h"
#include "sdkApi.h"
#include <serverListProvider.h>
#include <memory>
#include <chatd.h>
#include <db.h>
#include <buffer.h>
#include <chatdDb.h>
#include <megaapi_impl.h>
#include <autoHandle.h>
#include <asyncTools.h>
#include <codecvt> //for nonWhitespaceStr()
#include <locale>
#include "strongvelope/strongvelope.h"
#include "base64.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __ANDROID__
    #include <sys/system_properties.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #ifdef TARGET_OS_IPHONE
        #include <resolv.h>
    #endif
#endif

#define _QUICK_LOGIN_NO_RTC
using namespace promise;

namespace karere
{

template <class T, class F>
void callAfterInit(T* self, F&& func, void* ctx);

std::string encodeFirstName(const std::string& first);


/* Warning - the database is not initialzed at construction, but only after
 * init() is called. Therefore, no code in this constructor should access or
 * depend on the database
 */
    Client::Client(::mega::MegaApi& sdk, WebsocketsIO *websocketsIO, IApp& aApp, const std::string& appDir, uint8_t caps, void *ctx)
        : mAppDir(appDir),
          websocketIO(websocketsIO),
          appCtx(ctx),
          api(sdk, ctx),
          app(aApp),
          contactList(new ContactList(*this)),
          chats(new ChatRoomList(*this)),
          mMyName("\0", 1),
          mOwnPresence(Presence::kInvalid),
          mPresencedClient(&api, this, *this, caps)
{
}

KARERE_EXPORT const std::string& createAppDir(const char* dirname, const char *envVarName)
{
    static std::string path;
    if (!path.empty())
        return path;
    const char* dir = getenv(envVarName);
    if (dir)
    {
        path = dir;
    }
    else
    {
        const char* homedir = getenv(
            #ifndef _WIN32
                    "HOME"
            #else
                    "HOMEPATH"
            #endif
        );
        if (!homedir)
            throw std::runtime_error("Cant get HOME env variable");
        path = homedir;
        path.append("/").append(dirname);
    }
    struct stat info;
    auto ret = stat(path.c_str(), &info);
    if (ret == 0)
    {
        if ((info.st_mode & S_IFDIR) == 0)
            throw std::runtime_error("Application directory path is taken by a file");
    }
    else
    {
        ret = mkdir(path.c_str(), 0700);
        if (ret)
        {
            char buf[512];
#ifdef _WIN32
            strerror_s(buf, 511, ret);
#else
            (void)strerror_r(ret, buf, 511);
#endif
            buf[511] = 0; //just in case
            throw std::runtime_error(std::string("Error creating application directory: ")+buf);
        }
    }
    return path;
}

std::string Client::dbPath(const std::string& sid) const
{
    if (sid.size() < 50)
        throw std::runtime_error("dbPath: sid is too small");
    std::string path = mAppDir;
    path.reserve(56);
    path.append("/karere-").append(sid.c_str()+44).append(".db");
    return path;
}

bool Client::openDb(const std::string& sid)
{
    assert(!sid.empty());
    std::string path = dbPath(sid);
    struct stat info;
    bool exists = (stat(path.c_str(), &info) == 0);
    if (!exists)
    {
        KR_LOG_WARNING("Asked to use local cache, but it does not exist");
        return false;
    }

    bool ok = db.open(path.c_str(), false);
    if (!ok)
    {
        KR_LOG_WARNING("Error opening database");
        return false;
    }
    SqliteStmt stmt(db, "select value from vars where name = 'schema_version'");
    if (!stmt.step())
    {
        db.close();
        KR_LOG_WARNING("Can't get local database version");
        return false;
    }
    std::string ver(gDbSchemaHash);
    ver.append("_").append(gDbSchemaVersionSuffix);
    if (stmt.stringCol(0) != ver)
    {
        db.close();
        KR_LOG_WARNING("Database schema version is not compatible with app version, will rebuild it");
        return false;
    }
    mSid = sid;
    return true;
}

void Client::createDbSchema()
{
    mMyHandle = Id::null();
    db.simpleQuery(gDbSchema); //db.query() uses a prepared statement and will execute only the first statement up to the first semicolon
    std::string ver(gDbSchemaHash);
    ver.append("_").append(gDbSchemaVersionSuffix);
    db.query("insert into vars(name, value) values('schema_version', ?)", ver);
    db.commit();
}

void Client::heartbeat()
{
    if (db.isOpen())
    {
        db.timedCommit();
    }

    if (mConnState != kConnected)
    {
        KR_LOG_WARNING("Heartbeat timer tick without being connected");
        return;
    }
    mPresencedClient.heartbeat();
    //TODO: implement in chatd as well
}

Client::~Client()
{
    if (mHeartbeatTimer)
    {
        karere::cancelInterval(mHeartbeatTimer, appCtx);
        mHeartbeatTimer = 0;
    }
}

promise::Promise<void> Client::retryPendingConnections()
{
    if (mConnState == kConnecting)
        return mConnectPromise;

    std::vector<Promise<void>> promises;

    promises.push_back(mPresencedClient.retryPendingConnection());
    if (chatd)
    {
        promises.push_back(chatd->retryPendingConnections());
    }
    return promise::when(promises);
}

#define TOKENPASTE2(a,b) a##b
#define TOKENPASTE(a,b) TOKENPASTE2(a,b)

#define SHARED_STATE(varname, membtype)             \
    struct TOKENPASTE(SharedState,__LINE__){membtype value;};  \
    std::shared_ptr<TOKENPASTE(SharedState, __LINE__)> varname(new TOKENPASTE(SharedState,__LINE__))

//This is a convenience method to log in the SDK in case the app does not do it.
promise::Promise<void> Client::sdkLoginNewSession()
{
    mLoginDlg.assign(app.createLoginDialog());
    async::loop((int)0, [](int) { return true; }, [](int&){},
    [this](async::Loop<int>& loop)
    {
        auto pms = mLoginDlg->requestCredentials();
        return pms
        .then([this](const std::pair<std::string, std::string>& cred)
        {
            mLoginDlg->setState(IApp::ILoginDialog::kLoggingIn);
            return api.callIgnoreResult(&mega::MegaApi::login, cred.first.c_str(), cred.second.c_str());
        })
        .then([&loop]()
        {
            loop.breakLoop();
            return 0;
        })
        .fail([this](const promise::Error& err) -> Promise<int>
        {
            if (err.code() != mega::API_ENOENT && err.code() != mega::API_EARGS)
                return err;

            mLoginDlg->setState(IApp::ILoginDialog::kBadCredentials);
            return 0;
        });
    }, this)
    .then([this](int)
    {
        mLoginDlg->setState(IApp::ILoginDialog::kFetchingNodes);
        return api.callIgnoreResult(&::mega::MegaApi::fetchNodes);
    })
    .fail([this](const promise::Error& err)
    {
        auto wptr = weakHandle();
        marshallCall([wptr, this, err]()
        {
            if (wptr.deleted())
            {
                return;
            }

            mSessionReadyPromise.reject(err);
        }, appCtx);
    })
    .then([this]()
    {
        mLoginDlg.free();
    });
    return mSessionReadyPromise;
}

promise::Promise<void> Client::sdkLoginExistingSession(const char* sid)
{
    assert(sid);
    api.callIgnoreResult(&::mega::MegaApi::fastLogin, sid)
    .then([this]()
    {
        api.callIgnoreResult(&::mega::MegaApi::fetchNodes);
    });
    return mSessionReadyPromise;
}

promise::Promise<void> Client::loginSdkAndInit(const char* sid)
{
    init(sid);
    if (!sid)
    {
        return sdkLoginNewSession();
    }
    else
    {
        if (mInitState == kInitErrNoCache) //local karere cache not present or currupt, force sdk to do full fetchnodes
        {
            api.sdk.invalidateCache();
        }
        return sdkLoginExistingSession(sid);
    }
}

void Client::loadContactListFromApi()
{
    std::unique_ptr<::mega::MegaUserList> contacts(api.sdk.getContacts());
    loadContactListFromApi(*contacts);
}

void Client::loadContactListFromApi(::mega::MegaUserList& contacts)
{
#ifndef NDEBUG
    dumpContactList(contacts);
#endif
    contactList->syncWithApi(contacts);
    mContactsLoaded = true;
}

promise::Promise<void> Client::initWithNewSession(const char* sid, const std::string& scsn,
    const std::shared_ptr<::mega::MegaUserList>& contactList,
    const std::shared_ptr<::mega::MegaTextChatList>& chatList)
{
    assert(sid);

    mSid = sid;
    createDb();

// We have a complete snapshot of the SDK contact and chat list state.
// Commit it with the accompanying scsn
    mMyHandle = getMyHandleFromSdk();
    db.query("insert or replace into vars(name,value) values('my_handle', ?)", mMyHandle);

    mMyEmail = getMyEmailFromSdk();
    db.query("insert or replace into vars(name,value) values('my_email', ?)", mMyEmail);

    mUserAttrCache.reset(new UserAttrCache(*this));

    auto wptr = weakHandle();
    return loadOwnKeysFromApi()
    .then([this, scsn, contactList, chatList, wptr]()
    {
        if (wptr.deleted())
            return;
        loadContactListFromApi(*contactList);
        chatd.reset(new chatd::Client(this, mMyHandle));
        assert(chats->empty());
        chats->onChatsUpdate(*chatList);
        commit(scsn);
    });
}

void Client::commit(const std::string& scsn)
{
    if (scsn.empty())
    {
        KR_LOG_DEBUG("Committing with empty scsn");
        db.commit();
        return;
    }
    if (scsn == mLastScsn)
    {
        KR_LOG_DEBUG("Committing with same scsn");
        db.commit();
        return;
    }

    db.query("insert or replace into vars(name,value) values('scsn',?)", scsn);
    db.commit();
    mLastScsn = scsn;
    KR_LOG_DEBUG("Commit with scsn %s", scsn.c_str());
}

void Client::onEvent(::mega::MegaApi* api, ::mega::MegaEvent* event)
{
    assert(event);
    int type = event->getType();
    switch (type)
    {
    case ::mega::MegaEvent::EVENT_COMMIT_DB:
    {
        if (db)
        {
            auto pscsn = event->getText();
            if (!pscsn)
            {
                KR_LOG_WARNING("EVENT_COMMIT_DB with NULL scsn, ignoring");
                return;
            }
            std::string scsn = pscsn;
            auto wptr = weakHandle();
            marshallCall([wptr, this, scsn]()
            {
                if (wptr.deleted())
                {
                    return;
                }

                commit(scsn);
            }, appCtx);
        }
        break;
    }

    case ::mega::MegaEvent::EVENT_DISCONNECT:
    {
        if (connState() == kConnecting || connState() == kConnected)
        {
            auto wptr = weakHandle();
            marshallCall([wptr, this]()
            {
                if (wptr.deleted())
                {
                    return;
                }

                KR_LOG_WARNING("EVENT_DISCONNECT --> reconnect triggered by SDK");
                retryPendingConnections();

            }, appCtx);
        }
        break;
    }

    default:
        break;
    }
}

void Client::initWithDbSession(const char* sid)
{
    try
    {
        assert(sid);
        if (!openDb(sid))
        {
            assert(mSid.empty());
            setInitState(kInitErrNoCache);
            return;
        }
        assert(db);
        assert(!mSid.empty());
        mUserAttrCache.reset(new UserAttrCache(*this));

        mMyHandle = getMyHandleFromDb();
        assert(mMyHandle);

        mMyEmail = getMyEmailFromDb();

        mOwnNameAttrHandle = mUserAttrCache->getAttr(mMyHandle, USER_ATTR_FULLNAME, this,
        [](Buffer* buf, void* userp)
        {
            if (!buf || buf->empty())
                return;
            auto& name = static_cast<Client*>(userp)->mMyName;
            name.assign(buf->buf(), buf->dataSize());
        });

        loadOwnKeysFromDb();
        contactList->loadFromDb();
        mContactsLoaded = true;
        chatd.reset(new chatd::Client(this, mMyHandle));
        chats->loadFromDb();
    }
    catch(std::runtime_error& e)
    {
        KR_LOG_ERROR("initWithDbSession: Error loading session from local cache: %s", e.what());
        setInitState(kInitErrCorruptCache);
        return;
    }

    setInitState(kInitHasOfflineSession);
    return;
}

void Client::setInitState(InitState newState)
{
    if (newState == mInitState)
        return;
    mInitState = newState;
    KR_LOG_DEBUG("Client reached init state %s", initStateStr());
    app.onInitStateChange(mInitState);
}

Client::InitState Client::init(const char* sid)
{
    if (mInitState > kInitCreated)
    {
        KR_LOG_ERROR("init: karere is already initialized. Current state: %s", initStateStr());
        return kInitErrAlready;
    }

    api.sdk.addGlobalListener(this);

    if (sid)
    {
        initWithDbSession(sid);
        if (mInitState == kInitErrNoCache ||    // not found, uncompatible db version, cannot open
                mInitState == kInitErrCorruptCache)
        {
            wipeDb(sid);
        }
    }
    else
    {
        setInitState(kInitWaitingNewSession);
    }
    api.sdk.addRequestListener(this);
    return mInitState;
}

void Client::onRequestFinish(::mega::MegaApi* apiObj, ::mega::MegaRequest *request, ::mega::MegaError* e)
{
    if (e->getErrorCode() == mega::MegaError::API_ESID)
    {
        auto wptr = weakHandle();
        marshallCall([wptr, this]() // update state in the karere thread
        {
            if (wptr.deleted())
                return;

            if (initState() != kInitTerminated)
            {
                setInitState(kInitErrSidInvalid);
            }
        }, appCtx);
        return;
    }

    auto reqType = request->getType();
    switch (reqType)
    {
    case mega::MegaRequest::TYPE_LOGOUT:
    {
        if (request->getFlag() ||   // SDK has been logged out normally closing session
                request->getParamType() == mega::MegaError::API_ESID)   // SDK received ESID during login
        {
            auto wptr = weakHandle();
            marshallCall([wptr, this]() // update state in the karere thread
            {
                if (wptr.deleted())
                    return;

                if (initState() != kInitTerminated)
                {
                    setInitState(kInitErrSidInvalid);
                }
            }, appCtx);
            return;
        }
        break;
    }

    case mega::MegaRequest::TYPE_FETCH_NODES:
    {
        api.sdk.pauseActionPackets();
        auto state = mInitState;
        char* pscsn = api.sdk.getSequenceNumber();
        std::string scsn;
        if (pscsn)
        {
            scsn = pscsn;
            delete[] pscsn;
        }
        std::shared_ptr<::mega::MegaUserList> contactList(api.sdk.getContacts());
        std::shared_ptr<::mega::MegaTextChatList> chatList(api.sdk.getChatList());

        auto wptr = weakHandle();
        marshallCall([wptr, this, state, scsn, contactList, chatList]()
        {
            if (wptr.deleted())
                return;

            if (state == kInitHasOfflineSession)
            {
// disable this safety checkup, since dumpSession() differs from first-time login value
//              std::unique_ptr<char[]> sid(api.sdk.dumpSession());
//              assert(sid);
//              // we loaded our state from db
//              // verify the SDK sid is the same as ours
//              if (mSid != sid.get())
//              {
//                  setInitState(kInitErrSidMismatch);
//                  return;
//              }
                checkSyncWithSdkDb(scsn, *contactList, *chatList);
                setInitState(kInitHasOnlineSession);
                mSessionReadyPromise.resolve();
            }
            else if (state == kInitWaitingNewSession || state == kInitErrNoCache)
            {
                std::unique_ptr<char[]> sid(api.sdk.dumpSession());
                assert(sid);
                initWithNewSession(sid.get(), scsn, contactList, chatList)
                .fail([this](const promise::Error& err)
                {
                    mSessionReadyPromise.reject(err);
                    return err;
                })
                .then([this]()
                {
                    setInitState(kInitHasOnlineSession);
                    mSessionReadyPromise.resolve();
                });
            }
            api.sdk.resumeActionPackets();
        }, appCtx);
        break;
    }

    case mega::MegaRequest::TYPE_SET_ATTR_USER:
    {
        int attrType = request->getParamType();
        int changeType;
        if (attrType == mega::MegaApi::USER_ATTR_FIRSTNAME)
        {
            changeType = mega::MegaUser::CHANGE_TYPE_FIRSTNAME;
        }
        else if (attrType == mega::MegaApi::USER_ATTR_LASTNAME)
        {
            changeType = mega::MegaUser::CHANGE_TYPE_LASTNAME;
        }
        else
        {
            return;
        }

        auto wptr = weakHandle();
        marshallCall([wptr, this, changeType]()
        {
            if (wptr.deleted())
                return;

            mUserAttrCache->onUserAttrChange(mMyHandle, changeType);
        }, appCtx);
        break;
    }

    default:    // no action to be taken for other type of requests
    {
        break;
    }
    }
}

//TODO: We should actually wipe the whole app dir, but the log file may
//be in that dir, and it is in use
void Client::wipeDb(const std::string& sid)
{
    assert(!sid.empty());
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
    std::string path = dbPath(sid);
    remove(path.c_str());
    struct stat info;
    if (stat(path.c_str(), &info) == 0)
        throw std::runtime_error("wipeDb: Could not delete old database file in "+mAppDir);
}

void Client::createDb()
{
    wipeDb(mSid);
    std::string path = dbPath(mSid);
    if (!db.open(path.c_str(), false))
        throw std::runtime_error("Can't access application database at "+mAppDir);
    createDbSchema(); //calls commit() at the end
}

bool Client::checkSyncWithSdkDb(const std::string& scsn,
    ::mega::MegaUserList& contactList, ::mega::MegaTextChatList& chatList)
{
    SqliteStmt stmt(db, "select value from vars where name='scsn'");
    stmt.stepMustHaveData("get karere scsn");
    if (stmt.stringCol(0) == scsn)
    {
        KR_LOG_DEBUG("Db sync ok, karere scsn matches with the one from sdk");
        return true;
    }

    // We are not in sync, probably karere is one or more commits behind
    KR_LOG_WARNING("Karere db out of sync with sdk - scsn-s don't match. Will reload all state from SDK");

    // invalidate user attrib cache
    mUserAttrCache->invalidate();
    // sync contactlist first
    loadContactListFromApi(contactList);
    // sync the chatroom list
    chats->onChatsUpdate(chatList);
    // commit the snapshot
    commit(scsn);
    return false;
}

void Client::dumpChatrooms(::mega::MegaTextChatList& chatRooms)
{
    KR_LOG_DEBUG("=== Chatrooms received from API: ===");
    for (int i=0; i<chatRooms.size(); i++)
    {
        auto& room = *chatRooms.get(i);
        if (room.isGroup())
        {
            KR_LOG_DEBUG("%s(group, ownPriv=%s):",
                Id(room.getHandle()).toString().c_str(),
                privToString((chatd::Priv)room.getOwnPrivilege()));
        }
        else
        {
            KR_LOG_DEBUG("%s(1on1)", Id(room.getHandle()).toString().c_str());
        }
        auto peers = room.getPeerList();
        if (!peers)
        {
            KR_LOG_DEBUG("  (room has no peers)");
            continue;
        }
        for (int j = 0; j<peers->size(); j++)
            KR_LOG_DEBUG("  %s: %s", Id(peers->getPeerHandle(j)).toString().c_str(),
                privToString((chatd::Priv)peers->getPeerPrivilege(j)));
    }
    KR_LOG_DEBUG("=== Chatroom list end ===");
}
void Client::dumpContactList(::mega::MegaUserList& clist)
{
    KR_LOG_DEBUG("== Contactlist received from API: ==");
    for (int i=0; i< clist.size(); i++)
    {
        auto& user = *clist.get(i);
        auto visibility = user.getVisibility();
        if (visibility != ::mega::MegaUser::VISIBILITY_VISIBLE)
            KR_LOG_DEBUG("  %s (visibility = %d)", Id(user.getHandle()).toString().c_str(), visibility);
        else
            KR_LOG_DEBUG("  %s", Id(user.getHandle()).toString().c_str());
    }
    KR_LOG_DEBUG("== Contactlist end ==");
}

promise::Promise<void> Client::connect(Presence pres, bool isInBackground)
{
// only the first connect() needs to wait for the mSessionReadyPromise.
// Any subsequent connect()-s (preceded by disconnect()) can initiate
// the connect immediately
    if (mConnState == kConnecting)
        return mConnectPromise;
    else if (mConnState == kConnected)
        return promise::_Void();

    this->isInBackground = isInBackground;

    assert(mConnState == kDisconnected);
    auto sessDone = mSessionReadyPromise.done();    // wait for fetchnodes completion
    switch (sessDone)
    {
    case promise::kSucceeded:   // if session was already ready...
        return doConnect(pres);
    case promise::kFailed:
        return mSessionReadyPromise.error();
    default:                    // if session is not ready yet
        assert(sessDone == promise::kNotResolved);
        mConnectPromise = mSessionReadyPromise
            .then([this, pres]() mutable
            {
                return doConnect(pres);
            })
            .then([this]()
            {
                setConnState(kConnected);
            })
            .fail([this](const promise::Error& err)
            {
                setConnState(kDisconnected);
                return err;
            });
        return mConnectPromise;
    }
}

promise::Promise<void> Client::doConnect(Presence pres)
{
    assert(mSessionReadyPromise.succeeded());
    setConnState(kConnecting);
    mOwnPresence = pres;
    KR_LOG_DEBUG("Connecting to account '%s'(%s)...", SdkString(api.sdk.getMyEmail()).c_str(), mMyHandle.toString().c_str());
    assert(mUserAttrCache);
    mUserAttrCache->onLogin();
    mOwnNameAttrHandle = mUserAttrCache->getAttr(mMyHandle, USER_ATTR_FULLNAME, this,
    [](Buffer* buf, void* userp)
    {
        if (!buf || buf->empty())
            return;
        auto& name = static_cast<Client*>(userp)->mMyName;
        name.assign(buf->buf(), buf->dataSize());
        KR_LOG_DEBUG("Own screen name is: '%s'", name.c_str()+1);
    });

    connectToChatd();
    auto pms = connectToPresenced(mOwnPresence)
    .then([this]()
    {
        setConnState(kConnected);
    })
    .fail([this](const promise::Error& err)
    {
        setConnState(kDisconnected);
        return err;
    });
    assert(!mHeartbeatTimer);
    auto wptr = weakHandle();
    mHeartbeatTimer = karere::setInterval([this, wptr]()
    {
        if (wptr.deleted())
        {
            return;
        }

        if (!mHeartbeatTimer)
        {
            return;
        }

        heartbeat();
    }, 10000, appCtx);
    return pms;
}

void Client::disconnect()
{
    if (mConnState == kDisconnected)
        return;
    setConnState(kDisconnected);
    assert(mOwnNameAttrHandle.isValid());
    mUserAttrCache->removeCb(mOwnNameAttrHandle);
    mOwnNameAttrHandle = UserAttrCache::Handle::invalid();
    mUserAttrCache->onLogOut();
    if (mHeartbeatTimer)
    {
        karere::cancelInterval(mHeartbeatTimer, appCtx);
        mHeartbeatTimer = 0;
    }
    chatd->disconnect();
    mPresencedClient.disconnect();
}
void Client::setConnState(ConnState newState)
{
    mConnState = newState;
    KR_LOG_DEBUG("Client connection state changed to %s", connStateToStr(newState));
}
karere::Id Client::getMyHandleFromSdk()
{
    SdkString uh = api.sdk.getMyUserHandle();
    if (!uh.c_str() || !uh.c_str()[0])
        throw std::runtime_error("Could not get our own user handle from API");
    KR_LOG_INFO("Our user handle is %s", uh.c_str());
    karere::Id result(uh.c_str());
    if (result == Id::null() || result.val == ::mega::UNDEF)
        throw std::runtime_error("Own handle returned by the SDK is NULL");
    return result;
}

std::string Client::getMyEmailFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name='my_email'");
    if (!stmt.step())
        throw std::runtime_error("No own email in database");

    std::string email = stmt.stringCol(0);

    if (email.length() < 5)
        throw std::runtime_error("loadOwnEmailFromDb: Own email in db is invalid");
    return email;
}

std::string Client::getMyEmailFromSdk()
{
    SdkString myEmail = api.sdk.getMyEmail();
    if (!myEmail.c_str() || !myEmail.c_str()[0])
        throw std::runtime_error("Could not get our own email from API");
    KR_LOG_INFO("Our email address is %s", myEmail.c_str());
    return myEmail.c_str();
}

karere::Id Client::getMyHandleFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name='my_handle'");
    if (!stmt.step())
        throw std::runtime_error("No own user handle in database");

    karere::Id result = stmt.uint64Col(0);

    if (result == Id::null() || result.val == mega::UNDEF)
        throw std::runtime_error("loadOwnUserHandleFromDb: Own handle in db is invalid");
    return result;
}

promise::Promise<void> Client::loadOwnKeysFromApi()
{
    return api.call(&::mega::MegaApi::getUserAttribute, (int)mega::MegaApi::USER_ATTR_KEYRING)
    .then([this](ReqResult result) -> ApiPromise
    {
        auto keys = result->getMegaStringMap();
        auto cu25519 = keys->get("prCu255");
        if (!cu25519)
            return promise::Error("prCu255 private key missing in keyring from API");
        auto ed25519 = keys->get("prEd255");
        if (!ed25519)
            return promise::Error("prEd255 private key missing in keyring from API");

        auto b64len = strlen(cu25519);
        if (b64len != 43)
            return promise::Error("prCu255 base64 key length is not 43 bytes");
        base64urldecode(cu25519, b64len, mMyPrivCu25519, sizeof(mMyPrivCu25519));

        b64len = strlen(ed25519);
        if (b64len != 43)
            return promise::Error("prEd255 base64 key length is not 43 bytes");
        base64urldecode(ed25519, b64len, mMyPrivEd25519, sizeof(mMyPrivEd25519));
        return api.call(&mega::MegaApi::getUserData);
    })
    .then([this](ReqResult result) -> promise::Promise<void>
    {
        auto pubrsa = result->getPassword();
        if (!pubrsa)
            return promise::Error("No public RSA key in getUserData API response");
        mMyPubRsaLen = base64urldecode(pubrsa, strlen(pubrsa), mMyPubRsa, sizeof(mMyPubRsa));
        auto privrsa = result->getPrivateKey();
        if (!privrsa)
            return promise::Error("No private RSA key in getUserData API response");
        mMyPrivRsaLen = base64urldecode(privrsa, strlen(privrsa), mMyPrivRsa, sizeof(mMyPrivRsa));
        // write to db
        db.query("insert or replace into vars(name, value) values('pr_cu25519', ?)", StaticBuffer(mMyPrivCu25519, sizeof(mMyPrivCu25519)));
        db.query("insert or replace into vars(name, value) values('pr_ed25519', ?)", StaticBuffer(mMyPrivEd25519, sizeof(mMyPrivEd25519)));
        db.query("insert or replace into vars(name, value) values('pub_rsa', ?)", StaticBuffer(mMyPubRsa, mMyPubRsaLen));
        db.query("insert or replace into vars(name, value) values('pr_rsa', ?)", StaticBuffer(mMyPrivRsa, mMyPrivRsaLen));
        KR_LOG_DEBUG("loadOwnKeysFromApi: success");
        return promise::_Void();
    });
}

void Client::loadOwnKeysFromDb()
{
    SqliteStmt stmt(db, "select value from vars where name=?");

    stmt << "pr_rsa";
    stmt.stepMustHaveData();
    mMyPrivRsaLen = stmt.blobCol(0, mMyPrivRsa, sizeof(mMyPrivRsa));
    stmt.reset().clearBind();
    stmt << "pub_rsa";
    stmt.stepMustHaveData();
    mMyPubRsaLen = stmt.blobCol(0, mMyPubRsa, sizeof(mMyPubRsa));

    stmt.reset().clearBind();
    stmt << "pr_cu25519";
    stmt.stepMustHaveData();
    auto len = stmt.blobCol(0, mMyPrivCu25519, sizeof(mMyPrivCu25519));
    if (len != sizeof(mMyPrivCu25519))
        throw std::runtime_error("Unexpected length of privCu25519 in database");
    stmt.reset().clearBind();
    stmt << "pr_ed25519";
    stmt.stepMustHaveData();
    len = stmt.blobCol(0, mMyPrivEd25519, sizeof(mMyPrivEd25519));
    if (len != sizeof(mMyPrivEd25519))
        throw std::runtime_error("Unexpected length of privEd2519 in database");
}


promise::Promise<void> Client::connectToPresenced(Presence forcedPres)
{
    if (mPresencedUrl.empty())
    {
        return api.call(&::mega::MegaApi::getChatPresenceURL)
        .then([this, forcedPres](ReqResult result) -> Promise<void>
        {
            auto url = result->getLink();
            if (!url)
                return promise::Error("No presenced URL received from API");
            mPresencedUrl = url;
            return connectToPresencedWithUrl(mPresencedUrl, forcedPres);
        });
    }
    else
    {
        return connectToPresencedWithUrl(mPresencedUrl, forcedPres);
    }
}

promise::Promise<void> Client::connectToPresencedWithUrl(const std::string& url, Presence pres)
{
//we assume app.onOwnPresence(Presence::kOffline) has been called at application start
    presenced::IdRefMap peers;
    for (auto& contact: *contactList)
    {
        if (contact.second->visibility() == ::mega::MegaUser::VISIBILITY_VISIBLE)
            peers.insert(contact.first);
    }
    for (auto& chat: *chats)
    {
        if (!chat.second->isGroup())
            continue;
        auto& members = static_cast<GroupChatRoom*>(chat.second)->peers();
        for (auto& peer: members)
        {
            peers.insert(peer.first);
        }
    }
    if (pres.isValid())
    {
        mOwnPresence = pres;
        app.onPresenceChanged(mMyHandle, pres, true);
    }
    auto pmsPres = mPresencedClient.connect(url, mMyHandle, std::move(peers), presenced::Config(pres));
#ifndef KARERE_DISABLE_WEBRTC
// Create the rtc module
    rtc.reset(rtcModule::create(*this, *this, new rtcModule::RtcCrypto(*this), KARERE_DEFAULT_TURN_SERVERS));
    auto pmsRtc = rtc->init(10000);
    return promise::when(pmsPres, pmsRtc);
#else
    return pmsPres;
#endif
}

void Contact::updatePresence(Presence pres)
{
    mPresence = pres;
    updateAllOnlineDisplays(pres);
}
// presenced handlers
void Client::onPresenceChange(Id userid, Presence pres)
{
    if (userid == mMyHandle)
    {
        mOwnPresence = pres;
    }
    else
    {
        contactList->onPresenceChanged(userid, pres);
    }
    for (auto& item: *chats)
    {
        auto& chat = *item.second;
        if (!chat.isGroup())
            continue;
        static_cast<GroupChatRoom&>(chat).updatePeerPresence(userid, pres);
    }
    app.onPresenceChanged(userid, pres, false);
}
void Client::onPresenceConfigChanged(const presenced::Config& state, bool pending)
{
    app.onPresenceConfigChanged(state, pending);
}
void Client::onConnStateChange(presenced::Client::ConnState state)
{

}

void GroupChatRoom::updatePeerPresence(uint64_t userid, Presence pres)
{
    auto it = mPeers.find(userid);
    if (it == mPeers.end())
        return;
    it->second->mPresence = pres;
}

void Client::notifyNetworkOffline()
{
}
void Client::notifyNetworkOnline()
{
}
void Client::notifyUserIdle()
{
    if (chatd)
    {
        chatd->notifyUserIdle();
    }
}
void Client::notifyUserActive()
{
    if (chatd)
    {
        chatd->notifyUserActive();
    }
}

void Client::terminate(bool deleteDb)
{
    setInitState(kInitTerminated);

    api.sdk.removeRequestListener(this);
    api.sdk.removeGlobalListener(this);

#ifndef KARERE_DISABLE_WEBRTC
    if (rtc)
        rtc->hangupAll(rtcModule::TermCode::kAppTerminating);
#endif

    disconnect();
    mUserAttrCache.reset();

    if (deleteDb && !mSid.empty())
    {
        wipeDb(mSid);
    }
    else if (db)
    {
        KR_LOG_INFO("Doing final COMMIT to database");
        db.commit();
        db.close();
    }
}

promise::Promise<void> Client::setPresence(Presence pres)
{
    if (!mPresencedClient.setPresence(pres))
        return promise::Error("Not connected");
    else
    {
        app.onPresenceChanged(mMyHandle, pres, true);
        return promise::_Void();
    }
}

void Client::onUsersUpdate(mega::MegaApi* api, mega::MegaUserList *aUsers)
{
    if (!aUsers)
        return;
    std::shared_ptr<mega::MegaUserList> users(aUsers->copy());
    auto wptr = weakHandle();
    marshallCall([wptr, this, users]()
    {
        if (wptr.deleted())
        {
            return;
        }

        assert(mUserAttrCache);
        auto count = users->size();
        for (int i=0; i<count; i++)
        {
            auto& user = *users->get(i);
            if (user.getChanges())
            {
                if (user.isOwnChange() == 0)
                {
                    mUserAttrCache->onUserAttrChange(user);
                }
            }
            else
            {
                contactList->onUserAddRemove(user);
            }
        };
    }, appCtx);
}

promise::Promise<karere::Id>
Client::createGroupChat(std::vector<std::pair<uint64_t, chatd::Priv>> peers)
{
    std::unique_ptr<mega::MegaTextChatPeerList> sdkPeers(mega::MegaTextChatPeerList::createInstance());
    for (auto& peer: peers)
    {
        sdkPeers->addPeer(peer.first, peer.second);
    }
    return api.call(&mega::MegaApi::createChat, true, sdkPeers.get())
    .then([this](ReqResult result)->Promise<karere::Id>
    {
        auto& list = *result->getMegaTextChatList();
        if (list.size() < 1)
            throw std::runtime_error("Empty chat list returned from API");
        auto room = chats->addRoom(*list.get(0));
        if (!room || !room->isGroup())
            return promise::Error("API created incorrect group");
        room->connect();
        return karere::Id(room->chatid());
    });
}

promise::Promise<void> GroupChatRoom::excludeMember(uint64_t user)
{
    auto wptr = getDelTracker();
    return parent.client.api.callIgnoreResult(&mega::MegaApi::removeFromChat, chatid(), user)
    .then([this, wptr, user]()
    {
        wptr.throwIfDeleted();
        removeMember(user);
    });
}

ChatRoom::ChatRoom(ChatRoomList& aParent, const uint64_t& chatid, bool aIsGroup,
  unsigned char aShard, chatd::Priv aOwnPriv, uint32_t ts, const std::string& aTitle)
   :parent(aParent), mChatid(chatid),
    mShardNo(aShard), mIsGroup(aIsGroup),
    mOwnPriv(aOwnPriv), mTitleString(aTitle), mCreationTs(ts)
{}

//chatd::Listener
void ChatRoom::onLastMessageTsUpdated(uint32_t ts)
{
    callAfterInit(this, [this, ts]()
    {
        auto display = roomGui();
        if (display)
            display->onLastTsUpdated(ts);
    }, parent.client.appCtx);
}

ApiPromise ChatRoom::requestGrantAccess(mega::MegaNode *node, mega::MegaHandle userHandle)
{
    return parent.client.api.call(&::mega::MegaApi::grantAccessInChat, chatid(), node, userHandle);
}

ApiPromise ChatRoom::requestRevokeAccess(mega::MegaNode *node, mega::MegaHandle userHandle)
{
    return parent.client.api.call(&::mega::MegaApi::removeAccessInChat, chatid(), node, userHandle);
}

strongvelope::ProtocolHandler* Client::newStrongvelope(karere::Id chatid)
{
    return new strongvelope::ProtocolHandler(mMyHandle,
        StaticBuffer(mMyPrivCu25519, 32), StaticBuffer(mMyPrivEd25519, 32),
        StaticBuffer(mMyPrivRsa, mMyPrivRsaLen), *mUserAttrCache, db, chatid, appCtx);
}

void ChatRoom::createChatdChat(const karere::SetOfIds& initialUsers)
{
    mChat = &parent.client.chatd->createChat(
        mChatid, mShardNo, mUrl, this, initialUsers,
        parent.client.newStrongvelope(chatid()), mCreationTs, mIsGroup);
    if (mOwnPriv == chatd::PRIV_NOTPRESENT)
        mChat->disable(true);
}

template <class T, typename F>
void callAfterInit(T* self, F&& func, void *ctx)
{
    if (self->isInitializing())
    {
        auto wptr = self->weakHandle();
        marshallCall([wptr, func]()
        {
            if (!wptr.deleted())
                func();
        }, ctx);
    }
    else
    {
        func();
    }
}

void PeerChatRoom::initWithChatd()
{
    createChatdChat(SetOfIds({Id(mPeer), parent.client.myHandle()}));
}

void PeerChatRoom::connect()
{
    mChat->connect();
}

#ifndef KARERE_DISABLE_WEBRTC
rtcModule::ICall& ChatRoom::mediaCall(AvFlags av, rtcModule::ICallHandler& handler)
{
    return parent.client.rtc->startCall(chatid(), av, handler);
}
#endif

promise::Promise<void> PeerChatRoom::requesGrantAccessToNodes(mega::MegaNodeList *nodes)
{
    std::vector<ApiPromise> promises;

    for (int i = 0; i < nodes->size(); ++i)
    {
        if (!parent.client.api.sdk.hasAccessToAttachment(mChatid, nodes->get(i)->getHandle(), peer()))
        {
            ApiPromise promise = requestGrantAccess(nodes->get(i), peer());
            promises.push_back(promise);
        }
    }

    return promise::when(promises);
}

promise::Promise<void> PeerChatRoom::requestRevokeAccessToNode(mega::MegaNode *node)
{
    std::vector<ApiPromise> promises;

    mega::MegaHandleList *megaHandleList = parent.client.api.sdk.getAttachmentAccess(mChatid, node->getHandle());

    for (unsigned int j = 0; j < megaHandleList->size(); ++j)
    {
        ApiPromise promise = requestRevokeAccess(node, peer());
        promises.push_back(promise);
    }

    delete megaHandleList;

    return promise::when(promises);
}

promise::Promise<void> GroupChatRoom::requesGrantAccessToNodes(mega::MegaNodeList *nodes)
{
    std::vector<ApiPromise> promises;

    for (int i = 0; i < nodes->size(); ++i)
    {
        for (auto it = mPeers.begin(); it != mPeers.end(); ++it)
        {
            if (!parent.client.api.sdk.hasAccessToAttachment(mChatid, nodes->get(i)->getHandle(), it->second->mHandle))
            {
                ApiPromise promise = requestGrantAccess(nodes->get(i), it->second->mHandle);
                promises.push_back(promise);
            }
        }
    }

    return promise::when(promises);
}

promise::Promise<void> GroupChatRoom::requestRevokeAccessToNode(mega::MegaNode *node)
{
    std::vector<ApiPromise> promises;

    mega::MegaHandleList *megaHandleList = parent.client.api.sdk.getAttachmentAccess(mChatid, node->getHandle());

    for (unsigned int j = 0; j < megaHandleList->size(); ++j)
    {
        ApiPromise promise = requestRevokeAccess(node, megaHandleList->get(j));
        promises.push_back(promise);
    }

    delete megaHandleList;

    return promise::when(promises);
}

IApp::IGroupChatListItem* GroupChatRoom::addAppItem()
{
    auto list = parent.client.app.chatListHandler();
    return list ? list->addGroupChatItem(*this) : nullptr;
}

GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid,
    unsigned char aShard, chatd::Priv aOwnPriv, uint32_t ts, const std::string& title)
:ChatRoom(parent, chatid, true, aShard, aOwnPriv, ts, title),
mHasTitle(!title.empty()), mRoomGui(nullptr)
{
    SqliteStmt stmt(parent.client.db, "select userid, priv from chat_peers where chatid=?");
    stmt << mChatid;
    std::vector<promise::Promise<void> > promises;
    while(stmt.step())
    {
        promises.push_back(addMember(stmt.uint64Col(0), (chatd::Priv)stmt.intCol(1), false));
    }

    auto wptr = weakHandle();
    mMemberNamesResolved = promise::when(promises)
    .then([wptr, this]()
    {
        wptr.throwIfDeleted();
        if (!mHasTitle)
        {
            makeTitleFromMemberNames();
        }
    });

    notifyTitleChanged();
    initWithChatd();
    mRoomGui = addAppItem();
    mIsInitializing = false;
}

void GroupChatRoom::initWithChatd()
{
    karere::SetOfIds users;
    users.insert(parent.client.myHandle());
    for (auto& peer: mPeers)
    {
        users.insert(peer.first);
    }
    createChatdChat(users);
}

void GroupChatRoom::connect()
{
    if (chat().onlineState() != chatd::kChatStateOffline)
        return;

    mChat->connect();
    if (mHasTitle)
    {
        decryptTitle()
        .fail([](const promise::Error& err)
        {
            KR_LOG_DEBUG("Can't decrypt chatroom title. In function: GroupChatRoom::connect");
        });
    }
}

promise::Promise<void> GroupChatRoom::memberNamesResolved() const
{
    return mMemberNamesResolved;
}

IApp::IPeerChatListItem* PeerChatRoom::addAppItem()
{
    auto list = parent.client.app.chatListHandler();
    return list ? list->addPeerChatItem(*this) : nullptr;
}

PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid,
    unsigned char aShard, chatd::Priv aOwnPriv, const uint64_t& peer, chatd::Priv peerPriv, uint32_t ts)
:ChatRoom(parent, chatid, false, aShard, aOwnPriv, ts), mPeer(peer),
  mPeerPriv(peerPriv), mContact(*parent.client.contactList->contactFromUserId(peer)),
  mRoomGui(nullptr)
{
    //mTitleString is set by Contact::attachChatRoom() via updateTitle()
    mContact.attachChatRoom(*this); //defers title callbacks so they are not called during construction
    initWithChatd();
    mRoomGui = addAppItem();
    mIsInitializing = false;
}

PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat, Contact& contact)
    :ChatRoom(parent, chat.getHandle(), false, chat.getShard(),
     (chatd::Priv)chat.getOwnPrivilege(), chat.getCreationTime()),
    mPeer(getSdkRoomPeer(chat)), mPeerPriv(chatd::PRIV_RDONLY),
    mContact(contact),
    mRoomGui(nullptr)
{
    assert(!chat.isGroup());
    auto peers = chat.getPeerList();
    assert(peers);
    assert(peers->size() == 1);
    mPeer = peers->getPeerHandle(0);
    mPeerPriv = (chatd::Priv)peers->getPeerPrivilege(0);

    parent.client.db.query("insert into chats(chatid, shard, peer, peer_priv, own_priv, ts_created) values (?,?,?,?,?,?)",
        mChatid, mShardNo, mPeer, mPeerPriv, mOwnPriv, chat.getCreationTime());
//just in case
    parent.client.db.query("delete from chat_peers where chatid = ?", mChatid);

    mContact.attachChatRoom(*this);
    KR_LOG_DEBUG("Added 1on1 chatroom '%s' from API",  Id(mChatid).toString().c_str());
    initWithChatd();
    mRoomGui = addAppItem();
    mIsInitializing = false;
}
PeerChatRoom::~PeerChatRoom()
{
    if (mRoomGui && (parent.client.initState() != Client::kInitTerminated))
        parent.client.app.chatListHandler()->removePeerChatItem(*mRoomGui);
    auto chatd = parent.client.chatd.get();
    if (chatd)
        chatd->leave(mChatid);
}

uint64_t PeerChatRoom::getSdkRoomPeer(const ::mega::MegaTextChat& chat)
{
    auto& peers = *chat.getPeerList();
    assert(peers.size() == 1);
    return peers.getPeerHandle(0);
}

bool ChatRoom::syncOwnPriv(chatd::Priv priv)
{
    if (mOwnPriv == priv)
        return false;

    mOwnPriv = priv;
    parent.client.db.query("update chats set own_priv = ? where chatid = ?",
                priv, mChatid);
    return true;
}

bool PeerChatRoom::syncPeerPriv(chatd::Priv priv)
{
    if (mPeerPriv == priv)
        return false;
    mPeerPriv = priv;
    parent.client.db.query("update chats set peer_priv = ? where chatid = ?",
                priv, mChatid);
    return true;
}

bool PeerChatRoom::syncWithApi(const mega::MegaTextChat &chat)
{
    bool changed = ChatRoom::syncRoomPropertiesWithApi(chat);
    changed |= syncOwnPriv((chatd::Priv)chat.getOwnPrivilege());
    changed |= syncPeerPriv((chatd::Priv)chat.getPeerList()->getPeerPrivilege(0));
    return changed;
}

const std::string& PeerChatRoom::titleString() const
{
    return mTitleString;
}

promise::Promise<void> GroupChatRoom::addMember(uint64_t userid, chatd::Priv priv, bool saveToDb)
{
    assert(userid != parent.client.myHandle());

    auto it = mPeers.find(userid);
    if (it != mPeers.end())
    {
        if (it->second->mPriv == priv)
        {
            saveToDb = false;
        }
        else
        {
            it->second->mPriv = priv;
        }
    }
    else
    {
        mPeers.emplace(userid, new Member(*this, userid, priv)); //usernames will be updated when the Member object gets the username attribute

        if ((mOwnPriv != chatd::PRIV_NOTPRESENT) &&
           (parent.client.initState() >= Client::kInitHasOnlineSession))
            parent.client.presenced().addPeer(userid);
    }
    if (saveToDb)
    {
        parent.client.db.query("insert or replace into chat_peers(chatid, userid, priv) values(?,?,?)",
            mChatid, userid, priv);
    }

    return mPeers[userid]->nameResolved();
}

bool GroupChatRoom::removeMember(uint64_t userid)
{
    auto it = mPeers.find(userid);
    if (it == mPeers.end())
    {
        KR_LOG_WARNING("GroupChatRoom::removeMember for a member that we don't have, ignoring");
        return false;
    }
    delete it->second;
    mPeers.erase(it);
    parent.client.presenced().removePeer(userid);
    parent.client.db.query("delete from chat_peers where chatid=? and userid=?",
                mChatid, userid);
    if (!mHasTitle)
        makeTitleFromMemberNames();
    return true;
}

promise::Promise<void> GroupChatRoom::setPrivilege(karere::Id userid, chatd::Priv priv)
{
    assert(userid != parent.client.myHandle());
    auto wptr = getDelTracker();
    return parent.client.api.callIgnoreResult(&::mega::MegaApi::updateChatPermissions, chatid(), userid.val, priv)
    .then([this, wptr, userid, priv]()
    {
        wptr.throwIfDeleted();
        parent.client.db.query("update chat_peers set priv=? where chatid=? and userid=?", priv, mChatid, userid);
    });
}

promise::Promise<void> ChatRoom::truncateHistory(karere::Id msgId)
{
    auto wptr = getDelTracker();
    return parent.client.api.callIgnoreResult(
                &::mega::MegaApi::truncateChat,
                chatid(),
                msgId)
    .then([this, wptr]()
    {
        wptr.throwIfDeleted();
        // TODO: update indexes, last message and so on
    });
}

void GroupChatRoom::deleteSelf()
{
    //have to post a delete on the event loop, as there may be pending
    //events related to the chatroom/strongvelope instance
    auto wptr = weakHandle();
    marshallCall([wptr, this]()
    {
        if (wptr.deleted())
        {
            return;
        }

        auto db = parent.client.db;
        db.query("delete from chat_peers where chatid=?", mChatid);
        db.query("delete from chats where chatid=?", mChatid);
        delete this;
    }, parent.client.appCtx);
}

ChatRoomList::ChatRoomList(Client& aClient)
:client(aClient)
{}

void ChatRoomList::loadFromDb()
{
    SqliteStmt stmt(client.db, "select chatid, ts_created ,shard, own_priv, peer, peer_priv, title from chats");
    while(stmt.step())
    {
        auto chatid = stmt.uint64Col(0);
        if (find(chatid) != end())
        {
            KR_LOG_WARNING("ChatRoomList: Attempted to load from db cache a chatid that is already in memory");
            continue;
        }
        auto peer = stmt.uint64Col(4);
        ChatRoom* room;
        if (peer != uint64_t(-1))
            room = new PeerChatRoom(*this, chatid, stmt.intCol(2), (chatd::Priv)stmt.intCol(3), peer, (chatd::Priv)stmt.intCol(5), stmt.intCol(1));
        else
            room = new GroupChatRoom(*this, chatid, stmt.intCol(2), (chatd::Priv)stmt.intCol(3), stmt.intCol(1), stmt.stringCol(6));
        emplace(chatid, room);
    }
}
void ChatRoomList::addMissingRoomsFromApi(const mega::MegaTextChatList& rooms, SetOfIds& chatids)
{
    auto size = rooms.size();
    for (int i=0; i<size; i++)
    {
        auto& apiRoom = *rooms.get(i);
        bool isInactive = apiRoom.getOwnPrivilege()  == -1;
        auto chatid = apiRoom.getHandle();
        auto it = find(chatid);
        if (it != end())
            continue;
        KR_LOG_DEBUG("Adding %sroom %s from API",
            isInactive ? "(inactive) " : "",
            Id(apiRoom.getHandle()).toString().c_str());
        auto room = addRoom(apiRoom);
        if (!room)
            continue;
        chatids.insert(room->chatid());

        if (!isInactive && client.connState() == Client::kConnected)
        {
            KR_LOG_DEBUG("...connecting new room to chatd...");
            room->connect();
        }
        else
        {
            KR_LOG_DEBUG("...client is not connected or room is inactive, not connecting new room");
        }
    }
}

ChatRoom* ChatRoomList::addRoom(const mega::MegaTextChat& apiRoom)
{
    auto chatid = apiRoom.getHandle();

    ChatRoom* room;
    if(apiRoom.isGroup())
    {
        room = new GroupChatRoom(*this, apiRoom); //also writes it to cache
        if (client.connected())
        {
            GroupChatRoom *groupchat = static_cast<GroupChatRoom*>(room);
            if (groupchat->hasTitle())
            {
                groupchat->decryptTitle()
                .fail([](const promise::Error& err)
                {
                    KR_LOG_DEBUG("Can't decrypt chatroom title. In function: ChatRoomList::addRoom");
                });
            }
        }
    }
    else
    {
        auto peers = apiRoom.getPeerList();
        if (!peers)
        {
            KR_LOG_WARNING("addRoom: Ignoring 1on1 room %s with no peers", Id(apiRoom.getHandle()).toString().c_str());
            return nullptr;
        }
        if (peers->size() != 1)
        {
            KR_LOG_ERROR("addRoom: Trying to load a 1on1 room %s with more than one peer, ignoring room", Id(apiRoom.getHandle()).toString().c_str());
            return nullptr;
        }
        auto peer = apiRoom.getPeerList()->getPeerHandle(0);
        auto contact = client.contactList->contactFromUserId(peer);
        if (!contact) //trying to create a 1on1 chatroom with a user that is not a contact. handle it gracfully
        {
            KR_LOG_ERROR("addRoom: Trying to load a 1on1 chat with a non-contact %s, ignoring chatroom", karere::Id(peer).toString().c_str());
            return nullptr;
        }
        room = new PeerChatRoom(*this, apiRoom, *contact);
    }
#ifndef NDEBUG
    auto ret =
#endif
    emplace(chatid, room);
    assert(ret.second); //we should not have that room
    return room;
}

void ChatRoom::notifyExcludedFromChat()
{
    if (mAppChatHandler)
        mAppChatHandler->onExcludedFromChat();
    auto listItem = roomGui();
    if (listItem)
        listItem->onExcludedFromChat();
}

void ChatRoom::notifyRejoinedChat()
{
    if (mAppChatHandler)
        mAppChatHandler->onRejoinedChat();
    auto listItem = roomGui();
    if (listItem)
        listItem->onRejoinedChat();
}

void ChatRoomList::removeRoom(GroupChatRoom& room)
{
    auto it = find(room.chatid());
    if (it == end())
        throw std::runtime_error("removeRoom:: Room not in chat list");
    room.deleteSelf();
    erase(it);
}

void GroupChatRoom::setRemoved()
{
    mChat->disconnect();
    mOwnPriv = chatd::PRIV_NOTPRESENT;
    parent.client.db.query("update chats set own_priv=-1 where chatid=?", mChatid);
    notifyExcludedFromChat();
}

void Client::onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms)
{
    if (!rooms)
        return;
    std::shared_ptr<mega::MegaTextChatList> copy(rooms->copy());
    char* pscsn = api.sdk.getSequenceNumber();
    std::string scsn;
    if (pscsn)
    {
        scsn = pscsn;
        delete[] pscsn;
    }
#ifndef NDEBUG
    dumpChatrooms(*copy);
#endif
    assert(mContactsLoaded);
    auto wptr = weakHandle();
    marshallCall([wptr, this, copy, scsn]()
    {
        if (wptr.deleted())
        {
            return;
        }

        chats->onChatsUpdate(*copy);
    }, appCtx);
}

void ChatRoomList::onChatsUpdate(mega::MegaTextChatList& rooms)
{
    SetOfIds added;
    addMissingRoomsFromApi(rooms, added);
    auto count = rooms.size();
    for (int i = 0; i < count; i++)
    {
        auto apiRoom = rooms.get(i);
        auto chatid = apiRoom->getHandle();
        if (added.has(chatid)) //room was just added, no need to sync
            continue;
        auto it = find(chatid);
        auto localRoom = (it != end()) ? it->second : nullptr;
        auto priv = apiRoom->getOwnPrivilege();
        if (localRoom)
        {
            it->second->syncWithApi(*apiRoom);
        }
        else
        {   //we don't have the room locally, add it to local cache
            if (priv != chatd::PRIV_NOTPRESENT)
            {
                KR_LOG_DEBUG("Chatroom[%s]: Received new room",  Id(chatid).toString().c_str());
                auto room = addRoom(*apiRoom);
                if (!room)
                    continue;
                client.app.notifyInvited(*room);
                if (client.connected())
                {
                    room->connect();
                }
            }
            else
            {
                KR_LOG_DEBUG("Chatroom[%s]: Received new INACTIVE room",  Id(chatid).toString().c_str());
                auto room = addRoom(*apiRoom);
                if (!room)
                    continue;
                if (!room->isGroup())
                {
                    KR_LOG_ERROR("... inactive room is 1on1: only group chatrooms can be inactive");
                    continue;
                }
            }
        }
    }
}

ChatRoomList::~ChatRoomList()
{
    for (auto& room: *this)
        delete room.second;
}

GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& aChat)
:ChatRoom(parent, aChat.getHandle(), true, aChat.getShard(),
  (chatd::Priv)aChat.getOwnPrivilege(), aChat.getCreationTime()), mRoomGui(nullptr)
{
    auto title = aChat.getTitle();
    if (title && title[0])
    {
        mEncryptedTitle = title;
        mHasTitle = true;
    }
    else
    {
        mHasTitle = false;
    }

    auto peers = aChat.getPeerList();
    if (peers)
    {
        std::vector<promise::Promise<void> > promises;
        auto size = peers->size();
        for (int i=0; i<size; i++)
        {
            auto handle = peers->getPeerHandle(i);
            assert(handle != parent.client.myHandle());
            mPeers[handle] = new Member(*this, handle, (chatd::Priv)peers->getPeerPrivilege(i)); //may try to access mContactGui, but we have set it to nullptr, so it's ok
            promises.push_back(mPeers[handle]->nameResolved());
        }

        auto wptr = weakHandle();
        // If there is not any promise at vector promise, promise::when is resolved directly
        mMemberNamesResolved = promise::when(promises)
        .then([wptr, this]()
        {
            wptr.throwIfDeleted();
            if (!mHasTitle)
            {
                clearTitle();
            }
        });
    }
    else
    {
        if (!mHasTitle)
        {
            clearTitle();
        }
    }

    //save to db
    auto db = parent.client.db;
    db.query("delete from chat_peers where chatid=?", mChatid);
    db.query(
        "insert or replace into chats(chatid, shard, peer, peer_priv, "
        "own_priv, ts_created) values(?,?,-1,0,?,?)",
        mChatid, mShardNo, mOwnPriv, aChat.getCreationTime());

    SqliteStmt stmt(db, "insert into chat_peers(chatid, userid, priv) values(?,?,?)");
    for (auto& m: mPeers)
    {
        stmt << mChatid << m.first << m.second->mPriv;
        stmt.step();
        stmt.reset().clearBind();
    }

    initWithChatd();
    if (mOwnPriv != chatd::PRIV_NOTPRESENT)
        mRoomGui = addAppItem();
    mIsInitializing = false;
}

promise::Promise<void> GroupChatRoom::decryptTitle()
{
    if (mEncryptedTitle.empty())
    {
        return promise::_Void();
    }

    Buffer buf(mEncryptedTitle.size());
    size_t decLen;
    try
    {
        decLen = base64urldecode(mEncryptedTitle.c_str(), mEncryptedTitle.size(),
            buf.buf(), buf.bufSize());
    }
    catch(std::exception& e)
    {
        makeTitleFromMemberNames();
        std::string err("Error base64-decoding chat title: ");
        err.append(e.what()).append(". Falling back to member names");
        KR_LOG_ERROR("%s", err.c_str());
        return promise::Error(err);
    }

    buf.setDataSize(decLen);
    auto wptr = getDelTracker();
    return this->chat().crypto()->decryptChatTitle(buf)
    .then([wptr, this](const std::string& title)
    {
        wptr.throwIfDeleted();
        if (mTitleString == title)
        {
            KR_LOG_DEBUG("decryptTitle: Same title has been set, skipping update");
        }
        else
        {
            mTitleString = title;
            if (!mTitleString.empty())
            {
                mHasTitle = true;
                parent.client.db.query("update chats set title=? where chatid=?", mTitleString, mChatid);
            }
            else
            {
                clearTitle();
            }
        }

        notifyTitleChanged();
    })
    .fail([wptr, this](const promise::Error& err)
    {
        wptr.throwIfDeleted();
        KR_LOG_ERROR("Error decrypting chat title for chat %s:\n%s\nFalling back to member names.", karere::Id(chatid()).toString().c_str(), err.what());
        makeTitleFromMemberNames();
        return err;
    });
}

void GroupChatRoom::makeTitleFromMemberNames()
{
    mHasTitle = false;
    mTitleString.clear();
    if (mPeers.empty())
    {
        mTitleString = "(empty)";
    }
    else
    {
        for (auto& m: mPeers)
        {
            //name has binary layout
            auto& name = m.second->mName;
            assert(!name.empty()); //is initialized to '\0', so is never empty
            if (name.size() <= 1)
            {
                auto& email = m.second->mEmail;
                if (!email.empty())
                    mTitleString.append(email).append(", ");
                else
                    mTitleString.append("..., ");
            }
            else
            {
                mTitleString.append(name.substr(1)).append(", ");
            }
        }
        mTitleString.resize(mTitleString.size()-2); //truncate last ", "
    }
    assert(!mTitleString.empty());
    notifyTitleChanged();
}

void GroupChatRoom::loadTitleFromDb()
{
    //load user title if set
    SqliteStmt stmt(parent.client.db, "select title from chats where chatid = ?");
    stmt << mChatid;
    if (!stmt.step())
    {
        makeTitleFromMemberNames();
        return;
    }
    std::string strTitle = stmt.stringCol(0);
    if (strTitle.empty())
    {
        makeTitleFromMemberNames();
        return;
    }
    mTitleString = strTitle;
    mHasTitle = true;
}

promise::Promise<void> GroupChatRoom::setTitle(const std::string& title)
{
    auto wptr = getDelTracker();
    return chat().crypto()->encryptChatTitle(title)
    .then([wptr, this](const std::shared_ptr<Buffer>& buf)
    {
        wptr.throwIfDeleted();
        auto b64 = base64urlencode(buf->buf(), buf->dataSize());
        return parent.client.api.callIgnoreResult(&::mega::MegaApi::setChatTitle, chatid(),
            b64.c_str());
    })
    .then([wptr, this, title]()
    {
        wptr.throwIfDeleted();
        if (title.empty())
        {
            clearTitle();
        }
    });
}

GroupChatRoom::~GroupChatRoom()
{
    removeAppChatHandler();
    if (mRoomGui && (parent.client.initState() != Client::kInitTerminated))
        parent.client.app.chatListHandler()->removeGroupChatItem(*mRoomGui);

    auto chatd = parent.client.chatd.get();
    if (chatd)
        chatd->leave(mChatid);

    for (auto& m: mPeers)
    {
        delete m.second;
    }
}

promise::Promise<void> GroupChatRoom::leave()
{
    auto wptr = getDelTracker();
    return parent.client.api.callIgnoreResult(&mega::MegaApi::removeFromChat, mChatid, parent.client.myHandle())
    .fail([](const promise::Error& err) -> Promise<void>
    {
        if (err.code() == ::mega::MegaError::API_EARGS) //room does not actually exist on API, ignore room and remove it locally
            return promise::_Void();
        else
            return err;
    })
    .then([this, wptr]()
    {
        wptr.throwIfDeleted();
        setRemoved();
    });
}

promise::Promise<void> GroupChatRoom::invite(uint64_t userid, chatd::Priv priv)
{
    auto wptr = getDelTracker();
    promise::Promise<std::string> pms = mHasTitle
        ? chat().crypto()->encryptChatTitle(mTitleString, userid)
          .then([](const std::shared_ptr<Buffer>& buf)
          {
               return base64urlencode(buf->buf(), buf->dataSize());
          })
        : promise::Promise<std::string>(std::string());

    return pms
    .then([this, wptr, userid, priv](const std::string& title)
    {
        wptr.throwIfDeleted();
        return parent.client.api.call(&mega::MegaApi::inviteToChat, mChatid, userid, priv,
            title.empty() ? nullptr: title.c_str());
    })
    .then([this, wptr, userid, priv](ReqResult)
    {
        wptr.throwIfDeleted();
        addMember(userid, priv, true)
        .then([wptr, this]()
        {
            wptr.throwIfDeleted();
            if (!mHasTitle)
            {
                makeTitleFromMemberNames();
            }
        });
    });
}

bool ChatRoom::syncRoomPropertiesWithApi(const mega::MegaTextChat &chat)
{
    bool changed = false;
    if (chat.getShard() != mShardNo)
        throw std::runtime_error("syncWithApi: Shard number of chat can't change");
    if (chat.isGroup() != mIsGroup)
        throw std::runtime_error("syncWithApi: isGroup flag can't change");
    auto db = parent.client.db;
    chatd::Priv ownPriv = (chatd::Priv)chat.getOwnPrivilege();
    if (ownPriv != mOwnPriv)
    {
        mOwnPriv = ownPriv;
        changed = true;
        db.query("update chats set own_priv=? where chatid=?", ownPriv, mChatid);
        KR_LOG_DEBUG("Chatroom %s: own privilege updated from API", Id(mChatid).toString().c_str());
    }
    return changed;
}

//chatd::Listener::init
void ChatRoom::init(chatd::Chat& chat, chatd::DbInterface*& dbIntf)
{
    mChat = &chat;
    dbIntf = new ChatdSqliteDb(*mChat, parent.client.db);
    if (mAppChatHandler)
    {
        setAppChatHandler(mAppChatHandler);
    }
}

void ChatRoom::setAppChatHandler(IApp::IChatHandler* handler)
{
    if (mAppChatHandler)
        throw std::runtime_error("App chat handler is already set, remove it first");

    mAppChatHandler = handler;
    chatd::DbInterface* dummyIntf = nullptr;
// mAppChatHandler->init() may rely on some events, so we need to set mChatWindow as listener before
// calling init(). This is safe, as and we will not get any async events before we
//return to the event loop
    mChat->setListener(mAppChatHandler);
    mAppChatHandler->init(*mChat, dummyIntf);
}

void ChatRoom::removeAppChatHandler()
{
    if (!mAppChatHandler)
        return;
    mAppChatHandler = nullptr;
    mChat->setListener(this);
}

void GroupChatRoom::onUserJoin(Id userid, chatd::Priv privilege)
{
    if (userid == parent.client.myHandle())
    {
        syncOwnPriv(privilege);
    }
    else
    {
        auto wptr = weakHandle();
        addMember(userid, privilege, false)
        .then([wptr, this]()
        {
            wptr.throwIfDeleted();
            if (!mHasTitle)
            {
                makeTitleFromMemberNames();
            }
        });
    }
    if (mRoomGui)
    {
        mRoomGui->onUserJoin(userid, privilege);
    }
}

void GroupChatRoom::onUserLeave(Id userid)
{
    //TODO: We should handle leaving from the chatd event, not from API.
    if (userid == parent.client.myHandle())
        return;

    removeMember(userid);
    if (mRoomGui)
        mRoomGui->onUserLeave(userid);
}

void PeerChatRoom::onUserJoin(Id userid, chatd::Priv privilege)
{
    if (userid == parent.client.chatd->userId())
        syncOwnPriv(privilege);
    else if (userid.val == mPeer)
        syncPeerPriv(privilege);
    else
        KR_LOG_ERROR("PeerChatRoom: Bug: Received JOIN event from chatd for a third user, ignoring");
}
void PeerChatRoom::onUserLeave(Id userid)
{
    KR_LOG_ERROR("PeerChatRoom: Bug: Received an user leave event from chatd on a permanent chat, ignoring");
}

void ChatRoom::onLastTextMessageUpdated(const chatd::LastTextMsg& msg)
{
    if (mIsInitializing)
    {
        auto wptr = weakHandle();
        marshallCall([=]()
        {
            if (wptr.deleted())
                return;
            auto display = roomGui();
            if (display)
                display->onLastMessageUpdated(msg);
        }, parent.client.appCtx);
    }
    else
    {
        auto display = roomGui();
        if (display)
           display->onLastMessageUpdated(msg);
    }
}

//chatd notification
void ChatRoom::onOnlineStateChange(chatd::ChatState state)
{
    auto display = roomGui();
    if (display)
    {
        display->onChatOnlineState(state);
    }
}

void PeerChatRoom::onUnreadChanged()
{
    auto count = mChat->unreadMsgCount();
    if (mRoomGui)
        mRoomGui->onUnreadCountChanged(count);
    if (mContact.appItem())
        mContact.appItem()->onUnreadCountChanged(count);
}

void PeerChatRoom::updateTitle(const std::string& title)
{
    mTitleString = title;
    notifyTitleChanged();
}

void ChatRoom::notifyTitleChanged()
{
    callAfterInit(this, [this]
    {
        auto display = roomGui();
        if (display)
            display->onTitleChanged(mTitleString);

        if (mAppChatHandler)
            mAppChatHandler->onTitleChanged(mTitleString);
    }, parent.client.appCtx);
}

void GroupChatRoom::onUnreadChanged()
{
    auto count = mChat->unreadMsgCount();
    if (mRoomGui)
        mRoomGui->onUnreadCountChanged(count);
}

bool GroupChatRoom::syncMembers(const UserPrivMap& users)
{
    bool changed = false;
    auto db = parent.client.db;
    for (auto ourIt=mPeers.begin(); ourIt!=mPeers.end();)
    {
        auto userid = ourIt->first;
        auto it = users.find(userid);
        if (it == users.end()) //we have a user that is not in the chatroom anymore
        {
            changed = true;
            auto erased = ourIt;
            ourIt++;
            auto member = erased->second;
            mPeers.erase(erased);
            delete member;
            db.query("delete from chat_peers where chatid=? and userid=?", mChatid, userid);
            KR_LOG_DEBUG("GroupChatRoom[%s]:syncMembers: Removed member %s",
                 Id(mChatid).toString().c_str(),  Id(userid).toString().c_str());
        }
        else
        {
            if (ourIt->second->mPriv != it->second)
            {
                changed = true;
                db.query("update chat_peers set priv=? where chatid=? and userid=?",
                    it->second, mChatid, userid);
                KR_LOG_DEBUG("GroupChatRoom[%s]:syncMembers: Changed privilege of member %s: %d -> %d",
                     Id(chatid()).toString().c_str(), Id(userid).toString().c_str(),
                     ourIt->second->mPriv, it->second);
                ourIt->second->mPriv = it->second;
            }
            ourIt++;
        }
    }

    std::vector<promise::Promise<void> > promises;
    for (auto& user: users)
    {
        if (mPeers.find(user.first) == mPeers.end())
        {
            changed = true;
            promises.push_back(addMember(user.first, user.second, true));
        }
    }

    if (promises.size() > 0)
    {
        auto wptr = weakHandle();
        promise::when(promises)
        .then([wptr, this]()
        {
            wptr.throwIfDeleted();
            if (!mHasTitle)
            {
                makeTitleFromMemberNames();
            }
        });
    }

    return changed;
}
void GroupChatRoom::clearTitle()
{
    makeTitleFromMemberNames();
    parent.client.db.query("update chats set title=NULL where chatid=?", mChatid);
}

bool GroupChatRoom::syncWithApi(const mega::MegaTextChat& chat)
{
    auto oldPriv = mOwnPriv;
    bool changed = ChatRoom::syncRoomPropertiesWithApi(chat);
    UserPrivMap membs;
    changed |= syncMembers(apiMembersToMap(chat, membs));

    auto title = chat.getTitle();
    if (title && title[0])
    {
        mEncryptedTitle = title;
        mHasTitle = true;
        if (parent.client.connected())
        {
            decryptTitle()
            .fail([](const promise::Error& err)
            {
                KR_LOG_DEBUG("Can't decrypt chatroom title. In function: GroupChatRoom::syncWithApi. Error: %s", err.what());
            });
        }
    }
    else
    {
        // By checking if 'changed', we avoid some unnecessary notifications about title-updates
        // TODO: we still notify title-updates for all privilege changes, when only group
        // composition changes represent a title-update and should be notified
        if (changed)
        {
            clearTitle();
            KR_LOG_DEBUG("Empty title received for group chat %s", Id(mChatid).toString().c_str());
        }
    }

    if (!changed)
    {
        KR_LOG_DEBUG("Sync group chatroom %s with API: no changes", Id(mChatid).toString().c_str());
        return false;
    }

    if (oldPriv == chatd::PRIV_NOTPRESENT)
    {
        if (mOwnPriv != chatd::PRIV_NOTPRESENT)
        {
            //we were reinvited
            mChat->disable(false);
            notifyRejoinedChat();
            if (parent.client.connected())
                connect();
        }
    }
    else if (mOwnPriv == chatd::PRIV_NOTPRESENT)
    {
        //we were excluded
        KR_LOG_DEBUG("Chatroom[%s]: API event: We were removed",  Id(mChatid).toString().c_str());
        setRemoved(); // may delete 'this'
        return true;
    }
    KR_LOG_DEBUG("Synced group chatroom %s with API.", Id(mChatid).toString().c_str());
    return true;
}

UserPrivMap& GroupChatRoom::apiMembersToMap(const mega::MegaTextChat& chat, UserPrivMap& membs)
{
    auto members = chat.getPeerList();
    if (members)
    {
        auto size = members->size();
        for (int i=0; i<size; i++)
            membs.emplace(members->getPeerHandle(i), (chatd::Priv)members->getPeerPrivilege(i));
    }
    return membs;
}

GroupChatRoom::Member::Member(GroupChatRoom& aRoom, const uint64_t& user, chatd::Priv aPriv)
: mRoom(aRoom), mHandle(user), mPriv(aPriv), mName("\0", 1)
{
    mNameAttrCbHandle = mRoom.parent.client.userAttrCache().getAttr(
        user, USER_ATTR_FULLNAME, this,
        [](Buffer* buf, void* userp)
    {
        auto self = static_cast<Member*>(userp);
        if (buf && !buf->empty())
        {
            self->mName.assign(buf->buf(), buf->dataSize());
        }
        else
        {
            self->mName.assign("\0", 1);
        }
        if (self->mRoom.mAppChatHandler)
        {
            self->mRoom.mAppChatHandler->onMemberNameChanged(self->mHandle, self->mName);
        }

        if (!self->mNameResolved.done())
        {
            self->mNameResolved.resolve();
        }
        else if (self->mRoom.memberNamesResolved().done() && !self->mRoom.mHasTitle)
        {
            self->mRoom.makeTitleFromMemberNames();
        }
    });

    mEmailAttrCbHandle = mRoom.parent.client.userAttrCache().getAttr(
        user, USER_ATTR_EMAIL, this,
        [](Buffer* buf, void* userp)
    {
        auto self = static_cast<Member*>(userp);
        if (buf && !buf->empty())
        {
            self->mEmail.assign(buf->buf(), buf->dataSize());
            if (self->mName.size() <= 1 && self->mRoom.memberNamesResolved().done() && !self->mRoom.mHasTitle)
            {
                self->mRoom.makeTitleFromMemberNames();
            }
        }
    });
}

GroupChatRoom::Member::~Member()
{
    mRoom.parent.client.userAttrCache().removeCb(mNameAttrCbHandle);
    mRoom.parent.client.userAttrCache().removeCb(mEmailAttrCbHandle);
}

promise::Promise<void> GroupChatRoom::Member::nameResolved() const
{
    return mNameResolved;
}

void Client::connectToChatd()
{
    for (auto& item: *chats)
    {
        auto& chat = *item.second;
        if (!chat.chat().isDisabled())
            chat.connect();
    }
}

ContactList::ContactList(Client& aClient)
:client(aClient)
{}

void ContactList::loadFromDb()
{
    SqliteStmt stmt(client.db, "select userid, email, visibility, since from contacts");
    while(stmt.step())
    {
        auto userid = stmt.uint64Col(0);
        emplace(userid, new Contact(*this, userid, stmt.stringCol(1), stmt.intCol(2), stmt.int64Col(3),
            nullptr));
    }
}

bool ContactList::addUserFromApi(mega::MegaUser& user)
{
    auto userid = user.getHandle();
    auto& item = (*this)[userid];
    if (item)
    {
        int newVisibility = user.getVisibility();
        if (item->visibility() == newVisibility)
        {
            return false;
        }
        client.db.query("update contacts set visibility = ? where userid = ?",
            newVisibility, userid);
        item->onVisibilityChanged(newVisibility);
        return true;
    }
    auto cmail = user.getEmail();
    std::string email(cmail?cmail:"");
    int visibility = user.getVisibility();
    auto ts = user.getTimestamp();
    client.db.query("insert or replace into contacts(userid, email, visibility, since) values(?,?,?,?)",
            userid, email, visibility, ts);
    item = new Contact(*this, userid, email, visibility, ts, nullptr);
    KR_LOG_DEBUG("Added new user from API: %s", email.c_str());
    return true;
}

void Contact::onVisibilityChanged(int newVisibility)
{
    assert(newVisibility != mVisibility);
    auto old = mVisibility;
    mVisibility = newVisibility;
    if (mDisplay)
    {
        mDisplay->onVisibilityChanged(newVisibility);
    }

    auto& client = mClist.client;
    bool userDeleted = (newVisibility == ::mega::MegaUser::VISIBILITY_INACTIVE);
    if (newVisibility == ::mega::MegaUser::VISIBILITY_HIDDEN || userDeleted)
    {
        client.presenced().removePeer(mUserid, userDeleted);
        if (mChatRoom)
            mChatRoom->notifyExcludedFromChat();
    }
    else if (old == ::mega::MegaUser::VISIBILITY_HIDDEN && newVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE)
    {
        mClist.client.presenced().addPeer(mUserid);
        if (mChatRoom)
            mChatRoom->notifyRejoinedChat();
    }
}

void ContactList::syncWithApi(mega::MegaUserList& users)
{
    std::set<uint64_t> apiUsers;
    auto size = users.size();
    for (int i=0; i<size; i++)
    {
        auto& user = *users.get(i);
        if (user.getVisibility() == ::mega::MegaUser::VISIBILITY_INACTIVE)
        {
            continue;
        }
        apiUsers.insert(user.getHandle());
        addUserFromApi(user);
    }
    for (auto it = begin(); it!= end();)
    {
        auto handle = it->first;
        if (apiUsers.find(handle) != apiUsers.end())
        {
            it++;
            continue;
        }
        auto erased = it;
        it++;
        removeUser(erased);
    }
}

void ContactList::onUserAddRemove(mega::MegaUser& user)
{
    if (user.getVisibility() == ::mega::MegaUser::VISIBILITY_INACTIVE)
    {
        auto it = this->find(user.getHandle());
        if (it != this->end())
        {
            removeUser(it);
        }
    }
    else
    {
        addUserFromApi(user);
    }
}

void ContactList::removeUser(iterator it)
{
    auto handle = it->first;
    delete it->second;
    erase(it);
    client.db.query("delete from contacts where userid=?", handle);
}
void ContactList::onPresenceChanged(Id userid, Presence pres)
{
    auto it = find(userid);
    if (it == end())
        return;
    {
        it->second->updatePresence(pres);
    }
}
void ContactList::setAllOffline()
{
    for (auto& it: *this)
    {
        it.second->updatePresence(Presence::kOffline);
    }
}

promise::Promise<void> ContactList::removeContactFromServer(uint64_t userid)
{
    auto it = find(userid);
    if (it == end())
        return promise::Error("User "+karere::Id(userid).toString()+" not in contactlist");

    auto& api = client.api;
    std::unique_ptr<mega::MegaUser> user(api.sdk.getContact(it->second->email().c_str()));
    if (!user)
        return promise::Error("Could not get user object from email");
    //we don't remove it, we just set visibility to HIDDEN
    return api.callIgnoreResult(&::mega::MegaApi::removeContact, user.get());
}

ContactList::~ContactList()
{
    for (auto& it: *this)
        delete it.second;
}

const std::string* ContactList::getUserEmail(uint64_t userid) const
{
    auto it = find(userid);
    if (it == end())
        return nullptr;
    return &(it->second->email());
}

Contact* ContactList::contactFromEmail(const std::string &email) const
{
    for (auto it = begin(); it != end(); it++)
    {
        if (it->second->email() == email)
        {
            return it->second;
        }
    }
    return nullptr;
}

Contact* ContactList::contactFromUserId(uint64_t userid) const
{
    auto it = find(userid);
    return (it == end())? nullptr : it->second;
}

void Client::onContactRequestsUpdate(mega::MegaApi* api, mega::MegaContactRequestList* reqs)
{
    if (!reqs)
        return;

    std::shared_ptr<mega::MegaContactRequestList> copy(reqs->copy());
    auto wptr = weakHandle();
    marshallCall([wptr, this, copy]()
    {
        if (wptr.deleted())
        {
            return;
        }

        auto count = copy->size();
        for (int i=0; i<count; i++)
        {
            auto& req = *copy->get(i);
            if (req.isOutgoing())
                continue;
            if (req.getStatus() == mega::MegaContactRequest::STATUS_UNRESOLVED)
                app.onIncomingContactRequest(req);
        }
    }, appCtx);
}

Contact::Contact(ContactList& clist, const uint64_t& userid,
                 const std::string& email, int visibility,
                 int64_t since, PeerChatRoom* room)
    :mClist(clist), mUserid(userid), mChatRoom(room), mEmail(email),
     mSince(since), mVisibility(visibility)
{
    auto appClist = clist.client.app.contactListHandler();
    mDisplay = appClist ? appClist->addContactItem(*this) : nullptr;

    mUsernameAttrCbId = mClist.client.userAttrCache().getAttr(userid,
        USER_ATTR_FULLNAME, this,
        [](Buffer* data, void* userp)
        {
            //even if both first and last name are null, the data is at least
            //one byte - the firstname-size-prefix, which will be zero
            auto self = static_cast<Contact*>(userp);
            if (!data || data->empty() || (*data->buf() == 0))
                self->updateTitle(encodeFirstName(self->mEmail));
            else
                self->updateTitle(std::string(data->buf(), data->dataSize()));
        });
    if (mTitleString.empty()) // user attrib fetch was not synchornous
    {
        updateTitle(encodeFirstName(email));
        assert(!mTitleString.empty());
    }
    auto& client = mClist.client;
    if ((client.initState() >= Client::kInitHasOnlineSession)
         && (mVisibility == ::mega::MegaUser::VISIBILITY_VISIBLE))
        client.presenced().addPeer(mUserid);

    mIsInitializing = false;
}

// the title string starts with a byte equal to the first name length, followed by first name,
// then second name
void Contact::updateTitle(const std::string& str)
{
    mTitleString = str;
    notifyTitleChanged();
}

void Contact::notifyTitleChanged()
{
    callAfterInit(this, [this]
    {
        if (mDisplay)
            mDisplay->onTitleChanged(mTitleString);

        //1on1 chatrooms don't have a binary layout for the title
        if (mChatRoom)
            mChatRoom->updateTitle(mTitleString.substr(1));
    }, mClist.client.appCtx);
}

Contact::~Contact()
{
    auto& client = mClist.client;
    if (client.initState() != Client::kInitTerminated)
    {
        client.userAttrCache().removeCb(mUsernameAttrCbId);
        client.presenced().removePeer(mUserid, true);
        if (mDisplay)
        {
            client.app.contactListHandler()->removeContactItem(*mDisplay);
        }
    }
}

promise::Promise<ChatRoom*> Contact::createChatRoom()
{
    if (mChatRoom)
    {
        KR_LOG_WARNING("Contact::createChatRoom: chat room already exists, check before caling this method");
        return Promise<ChatRoom*>(mChatRoom);
    }
    mega::MegaTextChatPeerListPrivate peers;
    peers.addPeer(mUserid, chatd::PRIV_OPER);
    return mClist.client.api.call(&mega::MegaApi::createChat, false, &peers)
    .then([this](ReqResult result) -> Promise<ChatRoom*>
    {
        auto& list = *result->getMegaTextChatList();
        if (list.size() < 1)
            return promise::Error("Empty chat list returned from API");
        auto room = mClist.client.chats->addRoom(*list.get(0));
        if (!room)
            return promise::Error("API created an incorrect 1on1 room");
        room->connect();
        return room;
    });
}

void Contact::setChatRoom(PeerChatRoom& room)
{
    assert(!mChatRoom);
    assert(!mTitleString.empty());
    mChatRoom = &room;
    mChatRoom->updateTitle(mTitleString.substr(1));
}

void Contact::attachChatRoom(PeerChatRoom& room)
{
    if (mChatRoom)
        throw std::runtime_error("attachChatRoom[room "+Id(room.chatid()).toString()+ "]: contact "+
            Id(mUserid).toString()+" already has a chat room attached");
    KR_LOG_DEBUG("Attaching 1on1 chatroom %s to contact %s", Id(room.chatid()).toString().c_str(), Id(mUserid).toString().c_str());
    setChatRoom(room);
}
uint64_t Client::useridFromJid(const std::string& jid)
{
    auto end = jid.find('@');
    if (end != 13)
    {
        KR_LOG_WARNING("useridFromJid: Invalid Mega JID '%s'", jid.c_str());
        return mega::UNDEF;
    }

    uint64_t userid;
#ifndef NDEBUG
    auto len =
#endif
    mega::Base32::atob(jid.c_str(), (byte*)&userid, end);
    assert(len == 8);
    return userid;
}

Contact* ContactList::contactFromJid(const std::string& jid) const
{
    auto userid = Client::useridFromJid(jid);
    if (userid == mega::UNDEF)
        return nullptr;
    auto it = find(userid);
    if (it == this->end())
        return nullptr;
    else
        return it->second;
}

#define RETURN_ENUM_NAME(name) case name: return #name

const char* Client::initStateToStr(unsigned char state)
{
    switch (state)
    {
        RETURN_ENUM_NAME(kInitCreated);
        RETURN_ENUM_NAME(kInitWaitingNewSession);
        RETURN_ENUM_NAME(kInitHasOfflineSession);
        RETURN_ENUM_NAME(kInitHasOnlineSession);
        RETURN_ENUM_NAME(kInitTerminated);
        RETURN_ENUM_NAME(kInitErrGeneric);
        RETURN_ENUM_NAME(kInitErrNoCache);
        RETURN_ENUM_NAME(kInitErrCorruptCache);
        RETURN_ENUM_NAME(kInitErrSidMismatch);
        RETURN_ENUM_NAME(kInitErrSidInvalid);
    default:
        return "(unknown)";
    }
}
const char* Client::connStateToStr(ConnState state)
{
    switch(state)
    {
        RETURN_ENUM_NAME(kDisconnected);
        RETURN_ENUM_NAME(kConnecting);
        RETURN_ENUM_NAME(kConnected);
        default: return "(invalid)";
    }
}
#ifndef KARERE_DISABLE_WEBRTC
rtcModule::ICallHandler* Client::onCallIncoming(rtcModule::ICall& call, karere::AvFlags av)
{
    return app.onIncomingCall(call, av);
}
bool Client::onAnotherCall(rtcModule::ICall& existingCall, karere::Id userid)
{
    return true;
}
bool Client::isGroupChat(karere::Id chatid)
{
    auto it = chats->find(chatid);
    if (it == chats->end())
        throw std::runtime_error("Unknown chat "+chatid.toString());
    return it->second->isGroup();
}
#endif

std::string encodeFirstName(const std::string& first)
{
    std::string result;
    result.reserve(first.size()+1);
    result+=(char)(first.size());
    if (!first.empty())
    {
        result.append(first);
    }
    return result;
}

}
