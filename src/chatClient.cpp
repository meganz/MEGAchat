//we need the POSIX version of strerror_r, not the GNU one
#ifdef _GNU_SOURCE
    #undef _GNU_SOURCE
    #define _POSIX_C_SOURCE 201512L
#endif
#include <string.h>

#include "chatClient.h"
#include "contactList.h"
#include "ITypes.h" //for IPtr
#ifdef _WIN32
    #include <winsock2.h>
    #include <direct.h>
    #define mkdir(dir, mode) _mkdir(dir)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mstrophepp.h>
#include "rtcModule/IRtcModule.h"
#include "dummyCrypto.h" //for makeRandomString
#include "strophe.disco.h"
#include "base/services.h"
#include "sdkApi.h"
#include "megaCryptoFunctions.h"
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
//#include <chatdICrypto.h>
#include "strongvelope/strongvelope.h"
#include "chatRoom.h"
#include "base64.h"
#include <sys/types.h>
#include <sys/stat.h>

#define _QUICK_LOGIN_NO_RTC
using namespace promise;

namespace karere
{

void Client::sendPong(const std::string& peerJid, const std::string& messageId)
{
    strophe::Stanza pong(*conn);
    pong.setAttr("type", "result")
        .setAttr("to", peerJid)
        .setAttr("from", conn->fullJid())
        .setAttr("id", messageId);

    conn->send(pong);
}

/* Warning - the database is not initialzed at construction, but only after
 * init() is called. Therefore, no code in this constructor should access or
 * depend on the database
 */
Client::Client(::mega::MegaApi& sdk, IApp& aApp, const std::string& appDir,
               Presence pres)
 :mAppDir(appDir),
  conn(new strophe::Connection(services_strophe_get_ctx())),
  api(sdk), app(aApp),
  contactList(new ContactList(*this)),
  chats(new ChatRoomList(*this)),
  mOwnPresence(pres),
  mXmppContactList(*this),
  mXmppServerProvider(new XmppServerProvider("https://gelb530n001.karere.mega.nz", "xmpp", KARERE_FALLBACK_XMPP_SERVERS))
{
    try
    {
        app.onOwnPresence(Presence::kOffline);
    } catch(...){}
    api.sdk.addGlobalListener(this);
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

std::string Client::dbPath() const
{
    std::string path = mAppDir;

    path.append("/karere-").append(mSid.substr(44)).append(".db");
    return path;
}

void Client::openDb()
{
    std::string path = dbPath();
    struct stat info;
    bool exists = (stat(path.c_str(), &info) == 0);
    if (!exists)
        throw std::runtime_error("Asked to use local cache, but it does not exist");

    int ret = sqlite3_open(path.c_str(), &db);
    if (ret != SQLITE_OK || !db)
        throw std::runtime_error("Can't access application database at "+path);
}

void Client::createDbSchema(sqlite3*& database)
{
    mMyHandle = Id::null();
    MyAutoHandle<char*, void(*)(void*), sqlite3_free, (char*)nullptr> errmsg;
    int ret = sqlite3_exec(database, gKarereDbSchema, nullptr, nullptr, errmsg.handlePtr());
    if (ret)
    {
        if (errmsg)
            throw std::runtime_error("Error initializing database: "+std::string(errmsg));
        else
            throw std::runtime_error("Error "+std::to_string(ret)+" initializing database");
    }
}


Client::~Client()
{
    //when the strophe::Connection is destroyed, its handlers are automatically destroyed
}

#define TOKENPASTE2(a,b) a##b
#define TOKENPASTE(a,b) TOKENPASTE2(a,b)

#define SHARED_STATE(varname, membtype)             \
    struct TOKENPASTE(SharedState,__LINE__){membtype value;};  \
    std::shared_ptr<TOKENPASTE(SharedState, __LINE__)> varname(new TOKENPASTE(SharedState,__LINE__))

//This is a convenience method to log in the SDK in case the app does not do it.
promise::Promise<ReqResult> Client::sdkLoginNewSession()
{
    mLoginDlg.reset(app.createLoginDialog());
    return async::loop((int)0, [](int) { return true; }, [](int&){},
    [this](async::Loop<int>& loop)
    {
        return mLoginDlg->requestCredentials()
        .then([this](const std::pair<std::string, std::string>& cred)
        {
            mLoginDlg->setState(IApp::ILoginDialog::kLoggingIn);
            return api.call(&mega::MegaApi::login, cred.first.c_str(), cred.second.c_str());
        })
        .then([&loop](ReqResult res)
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
    })
    .then([this](int)
    {
        mLoginDlg->setState(IApp::ILoginDialog::kFetchingNodes);
        return api.call(&::mega::MegaApi::fetchNodes);
    })
    .then([this](ReqResult ret)
    {
        mLoginDlg.reset();
        return ret;
    });
}
promise::Promise<ReqResult> Client::sdkLoginExistingSession(const char* sid)
{
    assert(sid);
    return api.call(&::mega::MegaApi::fastLogin, sid)
    .then([this](ReqResult)
    {
        return api.call(&::mega::MegaApi::fetchNodes);
    });
}

promise::Promise<void> Client::loginSdkAndInit(const char* sid)
{
    if (!sid)
    {
        return sdkLoginNewSession()
        .then([this](ReqResult)
        {
            return initWithNewSession();
        });
    }
    else
    {
        return sdkLoginExistingSession(sid)
        .then([this](ReqResult)
        {
            initWithExistingSession();
        });
    }
}
void Client::loadContactListFromApi()
{
    auto contacts = api.sdk.getContacts();
    assert(contacts);
#ifndef NDEBUG
    dumpContactList(*contacts);
#endif
    contactList->syncWithApi(*contacts);
    mContactsLoaded = true;
}

promise::Promise<void> Client::initWithNewSession()
{
    const char* sid = api.sdk.dumpSession();
    assert(sid);

    mSid = sid;
    createDb();

    mMyHandle = getMyHandleFromSdk();
    sqliteQuery(db, "insert or replace into vars(name,value) values('my_handle', ?)", mMyHandle);

    mUserAttrCache.reset(new UserAttrCache(*this));

    return loadOwnKeysFromApi()
    .then([this]()
    {
        loadContactListFromApi();
        chatd.reset(new chatd::Client(mMyHandle));
        if (!mInitialChats.empty())
        {
            for (auto& list: mInitialChats)
            {
                chats->onChatsUpdate(list);
            }
            mInitialChats.clear();
        }
    });
}

promise::Promise<void> Client::initWithExistingSession()
{
    try
    {
        const char* sdkSid = api.sdk.dumpSession();
        assert(sdkSid);
        mSid = sdkSid;
        openDb();
        mUserAttrCache.reset(new UserAttrCache(*this));
        Id sdkHandle = getMyHandleFromSdk();
        Id dbHandle = getMyHandleFromDb();
        if (sdkHandle != dbHandle)
            throw std::runtime_error("initWithExistingSession: own handle from db does not matche the one from SDK");

        mMyHandle = sdkHandle;

        loadOwnKeysFromDb();
        contactList->loadFromDb();
        loadContactListFromApi();
        chatd.reset(new chatd::Client(mMyHandle));
        chats->loadFromDb();
    }
    KR_EXCEPTION_TO_PROMISE(KR);
    return promise:: Void();
}

promise::Promise<void> Client::init(bool useCache)
{
    try
    {
        if (useCache)
        {
            return initWithExistingSession();
        }
        else
        {
            return initWithNewSession();
        }
    }
    catch(std::exception& e)
    {
        //TODO: Handle promise::Exception
        return promise::Error(e.what());
    }
}
//TODO: We should actually wipe the whole app dir, but the log file may
//be in that dir, and it is in use
void Client::wipeDb()
{
    if (db)
    {
        sqlite3_close(db);
        db = nullptr;
    }
    std::string path = dbPath();
    remove(path.c_str());
    struct stat info;
    if (stat(path.c_str(), &info) == 0)
        throw std::runtime_error("wipeDb: Could not delete old database file in "+mAppDir);
}

void Client::createDb()
{
    wipeDb();
    std::string path = dbPath();
    int ret = sqlite3_open(path.c_str(), &db);
    if (ret != SQLITE_OK || !db)
        throw std::runtime_error("Can't access application database at "+mAppDir);
    createDbSchema(db);
}

void Client::dumpChatrooms(::mega::MegaTextChatList& chatRooms)
{
    KR_LOG_DEBUG("=== Chatrooms received from API: ===");
    for (int i=0; i<chatRooms.size(); i++)
    {
        auto& room = *chatRooms.get(i);
        if (room.isGroup())
        {
            auto url = room.getUrl();
            const char* noUrlMsg = (!url || (url[0] == 0) ? ", no url":"");
            KR_LOG_DEBUG("%s(group, ownPriv=%s%s):",
                Id(room.getHandle()).toString().c_str(),
                privToString((chatd::Priv)room.getOwnPrivilege()),
                noUrlMsg);
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

promise::Promise<void> Client::connect()
{
    KR_LOG_DEBUG("Login to Mega API successful");
    assert(mUserAttrCache);
    mUserAttrCache->onLogin();
    mUserAttrCache->getAttr(mMyHandle, mega::MegaApi::USER_ATTR_LASTNAME, this,
    [](Buffer* buf, void* userp)
    {
        if (buf)
            static_cast<Client*>(userp)->mMyName = buf->buf()+1;
    });

    connectToChatd();

    return mXmppServerProvider->getServer()
    .then([this](const std::shared_ptr<HostPortServerInfo>& server) mutable
    {
        return connectXmpp(server);
    });
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
        sqliteQuery(db, "insert into vars(name, value) values('pr_cu25519', ?)", StaticBuffer(mMyPrivCu25519, sizeof(mMyPrivCu25519)));
        sqliteQuery(db, "insert into vars(name, value) values('pr_ed25519', ?)", StaticBuffer(mMyPrivEd25519, sizeof(mMyPrivEd25519)));
        sqliteQuery(db, "insert into vars(name, value) values('pub_rsa', ?)", StaticBuffer(mMyPubRsa, mMyPubRsaLen));
        sqliteQuery(db, "insert into vars(name, value) values('pr_rsa', ?)", StaticBuffer(mMyPrivRsa, mMyPrivRsaLen));
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


promise::Promise<void> Client::connectXmpp(const std::shared_ptr<HostPortServerInfo>& server)
{
//we assume gui.onOwnPresence(Presence::kOffline) has been called at application start
    app.onOwnPresence(mOwnPresence.val() | Presence::kInProgress);
    assert(server);
    SdkString xmppPass = api.sdk.dumpXMPPSession();
    if (!xmppPass)
        return promise::Error("SDK returned NULL session id");
    if (xmppPass.size() < 16)
        throw std::runtime_error("Mega session id is shorter than 16 bytes");
    ((char&)xmppPass.c_str()[16]) = 0;

    //xmpp_conn_set_keepalive(*conn, 10, 4);
    // setup authentication information
    std::string jid = useridToJid(mMyHandle);
    jid.append("/kn_").append(rtcModule::makeRandomString(10));
    xmpp_conn_set_jid(*conn, jid.c_str());
    xmpp_conn_set_pass(*conn, xmppPass.c_str());
    KR_LOG_DEBUG("xmpp user = '%s', pass = '%s'", jid.c_str(), xmppPass.c_str());
    setupXmppHandlers();
    setupXmppReconnectHandler();
    Promise<void> pms = static_cast<Promise<void>&>(mReconnectController->start());
    return pms.then([this]()
    {
        KR_LOG_INFO("XMPP login successful");
// create and register disco strophe plugin
        conn->registerPlugin("disco", new disco::DiscoPlugin(*conn, "Karere Native"));

// Create and register the rtcmodule plugin
// the MegaCryptoFuncs object needs api.userData (to initialize the private key etc)
// To use DummyCrypto: new rtcModule::DummyCrypto(jid.c_str());
        rtc = rtcModule::create(*conn, this, new rtcModule::MegaCryptoFuncs(*this), KARERE_DEFAULT_TURN_SERVERS);
        conn->registerPlugin("rtcmodule", rtc);

/*
// create and register text chat plugin
        mTextModule = new TextModule(*this);
        conn->registerPlugin("textchat", mTextModule);
*/
        KR_LOG_DEBUG("webrtc plugin initialized");
        return mXmppContactList.ready();
    })
    .then([this]()
    {
        KR_LOG_DEBUG("XMPP contactlist initialized");
        //startKeepalivePings();
    });
}

void Client::setupXmppHandlers()
{
    conn->addHandler([this](strophe::Stanza stanza, void*, bool &keep) mutable
    {
            sendPong(stanza.attr("from"), stanza.attr("id"));
    }, "urn::xmpp::ping", "iq", nullptr, nullptr);
}

void Client::setupXmppReconnectHandler()
{
    mReconnectController.reset(karere::createRetryController("xmpp",
        [this](int no) -> promise::Promise<void>
    {
        if (no < 2)
        {
            auto& host = mXmppServerProvider->lastServer()->host;
            KR_LOG_INFO("Connecting to xmpp server %s...", host.c_str());
            app.onOwnPresence(mOwnPresence.val()|Presence::kInProgress);
            return conn->connect(host.c_str(), 0);
        }
        else
        {
            return mXmppServerProvider->getServer()
            .then([this](std::shared_ptr<HostPortServerInfo> server)
            {
                KR_LOG_WARNING("Connecting to new xmpp server: %s...", server->host.c_str());
                app.onOwnPresence(mOwnPresence | Presence::kInProgress);
                return conn->connect(server->host.c_str(), 0);
            });
        }
    },
    [this]()
    {
        xmpp_disconnect(*conn, -1);
        app.onOwnPresence(Presence::kOffline);
    },
    KARERE_LOGIN_TIMEOUT, 60000, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

    mReconnectConnStateHandler = conn->addConnStateHandler(
       [this](xmpp_conn_event_t event, int error,
        xmpp_stream_error_t* stream_error, bool& keepHandler) mutable
    {
        if (event == XMPP_CONN_CONNECT)
        {
            marshallCall([this]() //notify async, safer
            {
                mXmppContactList.fetch(); //waits for roster
                setPresence(mOwnPresence, true); //initiates roster fetch
                mXmppContactList.ready()
                .then([this]()
                {
                    app.onOwnPresence(mOwnPresence);
                });
            });
            return;
        }
        //we have a disconnect
        xmppContactList().notifyOffline();
        app.onOwnPresence(Presence::kOffline);

        if (mOwnPresence.status() == Presence::kOffline) //user wants to be offline
            return;
        if (isTerminating) //no need to handle
            return;
        assert(xmpp_conn_get_state(*conn) == XMPP_STATE_DISCONNECTED);
        if (mReconnectController->state() & rh::kStateBitRunning)
            return;

        if (mReconnectController->state() == rh::kStateFinished) //we had previous retry session, reset the retry controller
            mReconnectController->reset();
        mReconnectController->start(1); //need the 1ms delay to start asynchronously, in order to process(i.e. ignore) all stale libevent messages for the old connection so they don't get interpreted in the context of the new connection
    });
#if 0
    //test
    mega::setInterval([this]()
    {
        printf("simulating disconnect\n");
        xmpp_disconnect(*conn, -1);
    }, 6000);
#endif
}


void Client::notifyNetworkOffline()
{
    KR_LOG_WARNING("Network offline notification received, starting reconnect attempts");
    if (xmpp_conn_get_state(*conn) == XMPP_STATE_DISCONNECTED)
    {
        //if we are disconnected, the retry controller must never be at work, so not 'finished'
        assert(mReconnectController->state() != rh::kStateFinished);
        if (mReconnectController->currentAttemptNo() > 2)
            mReconnectController->restart();
    }
    else
    {
        conn->disconnect(-1); //this must trigger the conn state handler which will start the reconnect controller
    }
}


void Client::notifyNetworkOnline()
{
    if (xmpp_conn_get_state(*conn) == XMPP_STATE_CONNECTED)
        return;

    if (mReconnectController->state() == rh::kStateFinished)
    {
        KR_LOG_WARNING("notifyNetworkOnline: reconnect controller is in 'finished' state, but connection is not connected. Resetting reconnect controller.");
        mReconnectController->reset();
    }
    mReconnectController->restart();
}

promise::Promise<void> Client::terminate(bool deleteDb)
{
    if (isTerminating)
    {
        KR_LOG_WARNING("Client::terminate: Already terminating");
        return promise::Promise<void>();
    }
    isTerminating = true;
    api.sdk.removeGlobalListener(this);
    if (mReconnectConnStateHandler)
    {
        conn->removeConnStateHandler(mReconnectConnStateHandler);
        mReconnectConnStateHandler = 0;
    }
    if (mReconnectController)
        mReconnectController->abort();
    if (rtc)
        rtc->hangupAll();
    chatd.reset();
    if (deleteDb)
    {
        wipeDb();
    }
    else
    {
        sqlite3_close(db);
        db = nullptr;
    }
    promise::Promise<void> pms;
    conn->disconnect(2000)
    //resolve output promise asynchronously, because the callbacks of the output
    //promise may free the client, and the resolve()-s of the input promises
    //(mega and conn) are within the client's code, so any code after the resolve()s
    //that tries to access the client will crash
    .then([pms](int) mutable
    {
        marshallCall([pms]() mutable { pms.resolve(); });
    })
    .fail([pms](const promise::Error& err) mutable
    {
        marshallCall([pms, err]() mutable { pms.reject(err); });
        return err;
    });
    return pms;
}

void Client::startKeepalivePings()
{
    setInterval([this]()
    {
        if (!xmpp_conn_is_authenticated(*conn))
            return;
        if (mLastPingTs) //waiting for pong
        {
            if (xmpp_time_stamp()-mLastPingTs > 9000)
            {
                KR_LOG_WARNING("Keepalive ping timeout");
                notifyNetworkOffline();
            }
        }
        else
        {
            mLastPingTs = xmpp_time_stamp();
            pingPeer(nullptr)
            .then([this](strophe::Stanza s)
            {
                mLastPingTs = 0;
                return 0;
            });
        }
    }, 10000);
}


strophe::StanzaPromise Client::pingPeer(const char* peerJid)
{
    strophe::Stanza ping(*conn);
    ping.setName("iq")
        .setAttr("type", "get")
        .c("ping")
                .setAttr("xmlns", "urn:xmpp:ping");
    if (peerJid)
        ping.setAttr("to", peerJid);

    return conn->sendIqQuery(ping, "png")
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error receiving pong\n");
        return err;
    });
}

promise::Promise<void> Client::setPresence(Presence pres, bool force)
{
    if ((pres.status() == mOwnPresence.status()) && !force)
        return promise::Void();
    auto previous = mOwnPresence;
    mOwnPresence = pres;

    if (pres.status() == Presence::kOffline)
    {
        mReconnectController->abort();
        conn->disconnect(4000);
        app.onOwnPresence(Presence::kOffline);
        return promise::Void();
    }
    if (previous.status() == Presence::kOffline) //we were disconnected
    {
        mReconnectController->reset();
        app.onOwnPresence(pres.val() | Presence::kInProgress);
        return static_cast<promise::Promise<void>&>(mReconnectController->start());
    }
    app.onOwnPresence(pres.val() | Presence::kInProgress);
    strophe::Stanza msg(*conn);
    msg.setName("presence")
       .c("show")
           .t(pres.toString())
           .up()
       .c("status")
           .t(pres.toString())
           .up();

    return conn->sendQuery(msg)
    .then([this, pres](strophe::Stanza)
    {
        app.onOwnPresence(pres.status());
    });
}


void Client::onUsersUpdate(mega::MegaApi* api, mega::MegaUserList *aUsers)
{
    if (!aUsers)
        return;
    assert(mUserAttrCache);
    std::shared_ptr<mega::MegaUserList> users(aUsers->copy());
    marshallCall([this, users]()
    {
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
                contactList->onUserAddRemove(user);
        };
    });
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
    .then([this](ReqResult result)
    {
        auto& list = *result->getMegaTextChatList();
        if (list.size() < 1)
            throw std::runtime_error("Empty chat list returned from API");
        auto& room = chats->addRoom(*list.get(0));
        assert(room.isGroup());
        room.connect();
        return karere::Id(room.chatid());
    });
}

promise::Promise<ReqResult> GroupChatRoom::excludeMember(uint64_t user)
{
    return parent.client.api.call(&mega::MegaApi::removeFromChat, chatid(), user);
}

ChatRoom::ChatRoom(ChatRoomList& aParent, const uint64_t& chatid, bool aIsGroup, const std::string& aUrl,
  unsigned char aShard, chatd::Priv aOwnPriv)
:parent(aParent), mChatid(chatid), mUrl(aUrl), mShardNo(aShard), mIsGroup(aIsGroup),
  mOwnPriv(aOwnPriv)
{}

strongvelope::ProtocolHandler* Client::newStrongvelope(karere::Id chatid)
{
    return new strongvelope::ProtocolHandler(mMyHandle,
        StaticBuffer(mMyPrivCu25519, 32), StaticBuffer(mMyPrivEd25519, 32),
        StaticBuffer(mMyPrivRsa, mMyPrivRsaLen), *mUserAttrCache, db, chatid);
}

void ChatRoom::createChatdChat(const karere::SetOfIds& initialUsers)
{
    mChat = &parent.client.chatd->createChat(
        mChatid, mShardNo, mUrl, this, initialUsers,
        parent.client.newStrongvelope(chatid()));
}

void PeerChatRoom::initWithChatd()
{
    createChatdChat(SetOfIds({Id(mPeer), parent.client.myHandle()}));
}

void PeerChatRoom::connect()
{
    updateUrl()
    .then([this]()
    {
        mChat->connect();
    });
}

promise::Promise<void> PeerChatRoom::mediaCall(AvFlags av)
{
    assert(mAppChatHandler);
    auto jid = karere::useridToJid(mPeer);
    parent.client.rtc->startMediaCall(mAppChatHandler->callHandler(), jid, av);
    return promise::_Void();
}

promise::Promise<void> GroupChatRoom::mediaCall(AvFlags av)
{
    return promise::Error("Group chat calls are not implemented yet");
}

IApp::IGroupChatListItem* GroupChatRoom::addAppItem()
{
    auto list = parent.client.app.chatListHandler();
    return list ? list->addGroupChatItem(*this) : nullptr;
}

GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl, unsigned char aShard,
    chatd::Priv aOwnPriv, const std::string& title)
:ChatRoom(parent, chatid, true, aUrl, aShard, aOwnPriv), mRoomGui(addAppItem()),
  mTitleString(title), mHasTitle(!title.empty())
{
    SqliteStmt stmt(parent.client.db, "select userid, priv from chat_peers where chatid=?");
    stmt << mChatid;
    while(stmt.step())
    {
        addMember(stmt.uint64Col(0), (chatd::Priv)stmt.intCol(1), false);
    }
    if (!mTitleString.empty() && mRoomGui)
    {
        mRoomGui->onTitleChanged(mTitleString);
    }
    initWithChatd();
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
    updateUrl()
    .then([this]()
    {
        mChat->connect();
        decryptTitle();
    });
}

IApp::IPeerChatListItem* PeerChatRoom::addAppItem()
{
    auto list = parent.client.app.chatListHandler();
    return list ? list->addPeerChatItem(*this) : nullptr;
}

PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl,
    unsigned char aShard, chatd::Priv aOwnPriv, const uint64_t& peer, chatd::Priv peerPriv)
:ChatRoom(parent, chatid, false, aUrl, aShard, aOwnPriv), mPeer(peer),
  mPeerPriv(peerPriv), mContact(parent.client.contactList->contactFromUserId(peer)),
  mRoomGui(addAppItem())
{
    mContact.attachChatRoom(*this);
    initWithChatd();
}

PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat)
    :ChatRoom(parent, chat.getHandle(), false, chat.getUrl(), chat.getShard(),
     (chatd::Priv)chat.getOwnPrivilege()),
    mPeer(getSdkRoomPeer(chat)), mPeerPriv(chatd::PRIV_RDONLY),
    mContact(parent.client.contactList->contactFromUserId(mPeer)),
    mRoomGui(addAppItem())
{
    assert(!chat.isGroup());
    auto peers = chat.getPeerList();
    assert(peers);
    assert(peers->size() == 1);
    mPeer = peers->getPeerHandle(0);
    mPeerPriv = (chatd::Priv)peers->getPeerPrivilege(0);

    sqliteQuery(parent.client.db, "insert into chats(chatid, url, shard, peer, peer_priv, own_priv) values (?,?,?,?,?,?)",
        mChatid, mUrl, mShardNo, mPeer, mPeerPriv, mOwnPriv);
//just in case
    sqliteQuery(parent.client.db, "delete from chat_peers where chatid = ?", mChatid);
    mContact.attachChatRoom(*this);
    KR_LOG_DEBUG("Added 1on1 chatroom '%s' from API",  Id(mChatid).toString().c_str());
    initWithChatd();
}

uint64_t PeerChatRoom::getSdkRoomPeer(const ::mega::MegaTextChat& chat)
{
    auto& peers = *chat.getPeerList();
    assert(peers.size() == 1);
    return peers.getPeerHandle(0);
}

bool PeerChatRoom::syncOwnPriv(chatd::Priv priv)
{
    if (mOwnPriv == priv)
        return false;

    mOwnPriv = priv;
    sqliteQuery(parent.client.db, "update chats set own_priv = ? where chatid = ?",
                priv, mChatid);
    return true;
}

bool PeerChatRoom::syncPeerPriv(chatd::Priv priv)
{
    if (mPeerPriv == priv)
        return false;
    mPeerPriv = priv;
    sqliteQuery(parent.client.db, "update chats set peer_priv = ? where chatid = ?",
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
    return mContact.titleString();
}

void GroupChatRoom::addMember(const uint64_t& userid, chatd::Priv priv, bool saveToDb)
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
    }
    if (saveToDb)
    {
        sqliteQuery(parent.client.db, "insert or replace into chat_peers(chatid, userid, priv) values(?,?,?)",
            mChatid, userid, priv);
    }
    if (!mHasTitle)
        makeTitleFromMemberNames();
}

bool GroupChatRoom::removeMember(const uint64_t& userid)
{
    auto it = mPeers.find(userid);
    if (it == mPeers.end())
    {
        KR_LOG_WARNING("GroupChatRoom::removeMember for a member that we don't have, ignoring");
        return false;
    }
    delete it->second;
    mPeers.erase(it);
    sqliteQuery(parent.client.db, "delete from chat_peers where chatid=? and userid=?",
                mChatid, userid);
    if (!mHasTitle)
        makeTitleFromMemberNames();
    return true;
}

promise::Promise<ReqResult> GroupChatRoom::setPrivilege(karere::Id userid, chatd::Priv priv)
{
    return parent.client.api.call(&::mega::MegaApi::updateChatPermissions, chatid(), userid.val, priv);
}

void GroupChatRoom::deleteSelf()
{
    auto db = parent.client.db;
    sqliteQuery(db, "delete from chat_peers where chatid=?", mChatid);
    sqliteQuery(db, "delete from chats where chatid=?", mChatid);
    delete this;
}

promise::Promise<void> ChatRoom::updateUrl()
{
    return parent.client.api.call(&mega::MegaApi::getUrlChat, mChatid)
    .then([this](ReqResult result)
    {
        const char* url = result->getLink();
        if (!url || !url[0])
            return;
        std::string sUrl = url;
        if (sUrl == mUrl)
            return;
        mUrl = sUrl;
        sqliteQuery(parent.client.db, "update chats set url=? where chatid=?", mUrl, mChatid);
        KR_LOG_DEBUG("Updated chatroom %s url", Id(mChatid).toString().c_str());
        return;
    });
}

ChatRoomList::ChatRoomList(Client& aClient)
:client(aClient)
{}

void ChatRoomList::loadFromDb()
{
    SqliteStmt stmt(client.db, "select chatid, url, shard, own_priv, peer, peer_priv, title from chats");
    while(stmt.step())
    {
        auto chatid = stmt.uint64Col(0);
        if (find(chatid) != end())
        {
            KR_LOG_WARNING("ChatRoomList: Attempted to load from db cache a chatid that is already in memory");
            continue;
        }
        auto url = stmt.stringCol(1);
        if (url.empty())
        {
            KR_LOG_ERROR("ChatRoomList::loadFromDb: Chatroom has empty URL, ignoring and deleting from db");
            sqliteQuery(client.db, "delete from chats where chatid = ?", chatid);
            sqliteQuery(client.db, "delete from chat_peers where chatid = ?", chatid);
            continue;
        }
        auto peer = stmt.uint64Col(4);
        ChatRoom* room;
        if (peer != uint64_t(-1))
            room = new PeerChatRoom(*this, chatid, stmt.stringCol(1), stmt.intCol(2), (chatd::Priv)stmt.intCol(3), peer, (chatd::Priv)stmt.intCol(5));
        else
            room = new GroupChatRoom(*this, chatid, stmt.stringCol(1), stmt.intCol(2), (chatd::Priv)stmt.intCol(3), stmt.stringCol(6));
        emplace(chatid, room);
    }
}
void ChatRoomList::addMissingRoomsFromApi(const mega::MegaTextChatList& rooms)
{
    auto size = rooms.size();
    for (int i=0; i<size; i++)
    {
        auto& room = *rooms.get(i);
        if (room.getOwnPrivilege() == -1)
        {
            KR_LOG_DEBUG("Chatroom %s is inactive, skipping", Id(room.getHandle()).toString().c_str());
            continue;
        }
        addRoom(room);
    }
}

ChatRoom& ChatRoomList::addRoom(const mega::MegaTextChat& apiRoom)
{
    auto chatid = apiRoom.getHandle();
    auto it = find(chatid);
    if (it != end()) //we already have that room
    {
        return *it->second;
    }
    ChatRoom* room;
    if(apiRoom.isGroup())
    {
        room = new GroupChatRoom(*this, apiRoom); //also writes it to cache
    }
    else
    {
        assert(apiRoom.getPeerList()->size() == 1);
        room = new PeerChatRoom(*this, apiRoom);
    }
    emplace(chatid, room);
    return *room;
}

bool ChatRoomList::removeRoom(const uint64_t &chatid)
{
    auto it = find(chatid);
    if (it == end())
        return false;
    if (!it->second->isGroup())
        throw std::runtime_error("Can't delete a 1on1 chat");
    static_cast<GroupChatRoom*>(it->second)->deleteSelf();
    erase(it);
    return true;
}

void Client::onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList* rooms)
{
    std::shared_ptr<mega::MegaTextChatList> copy(rooms->copy());
#ifndef NDEBUG
    dumpChatrooms(*copy);
#endif
    if (!mContactsLoaded)
    {
        marshallCall([this, copy]()
        {
            KR_LOG_DEBUG("onChatsUpdate: no contactlist yet, caching the update info");
            mInitialChats.push_back(copy);
        });
    }
    else
    {
        marshallCall([this, copy]()
        {
            chats->onChatsUpdate(copy);
        });
    }
}

void ChatRoomList::onChatsUpdate(const std::shared_ptr<mega::MegaTextChatList>& rooms)
{
    addMissingRoomsFromApi(*rooms);
    auto count = rooms->size();
    for (int i = 0; i < count; i++)
    {
        std::shared_ptr<const ::mega::MegaTextChat> room(rooms->get(i));
        auto chatid = room->getHandle();
        auto it = find(chatid);
        auto localRoom = (it != end()) ? it->second : nullptr;
        auto priv = room->getOwnPrivilege();
        if (localRoom)
        {
            if (priv == chatd::PRIV_NOTPRESENT) //we were removed by someone else
            {
                KR_LOG_DEBUG("Chatroom[%s]: API event: We were removed",  Id(chatid).toString().c_str());
                removeRoom(chatid);
                continue;
            }
            else
            {   //we have the room, there maybe is some change on room properties
                it->second->syncWithApi(*room);
            }
        }
        else
        {   //we don't have the room locally
            if (priv != chatd::PRIV_NOTPRESENT)
            {
                //we are in the room, add it to local cache
                KR_LOG_DEBUG("Chatroom[%s]: Received invite to join",  Id(chatid).toString().c_str());
                client.app.notifyInvited(addRoom(*room));
            }
            else
            {   //we don't have the room, and we are not in the room - we have just removed ourselves from it, and deleted it locally
                KR_LOG_DEBUG("Chatroom[%s]: We should have just removed ourself from the room",  Id(chatid).toString().c_str());
                continue;
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
:ChatRoom(parent, aChat.getHandle(), true, aChat.getUrl(), aChat.getShard(),
  (chatd::Priv)aChat.getOwnPrivilege()), mRoomGui(addAppItem()), mHasTitle(false)
{
    auto peers = aChat.getPeerList();
    if (peers)
    {
        auto size = peers->size();
        for (int i=0; i<size; i++)
        {
            auto handle = peers->getPeerHandle(i);
            mPeers[handle] = new Member(*this, handle, (chatd::Priv)peers->getPeerPrivilege(i)); //may try to access mContactGui, but we have set it to nullptr, so it's ok
        }
    }
//save to db
    auto db = parent.client.db;
    sqliteQuery(db, "delete from chat_peers where chatid=?", mChatid);
    sqliteQuery(db, "insert or replace into chats(chatid, url, shard, peer, peer_priv, own_priv) values(?,?,?,-1,0,?)",
        mChatid, mUrl, mShardNo, mOwnPriv);

    SqliteStmt stmt(db, "insert into chat_peers(chatid, userid, priv) values(?,?,?)");
    for (auto& m: mPeers)
    {
        stmt << mChatid << m.first << m.second->mPriv;
        stmt.step();
        stmt.reset().clearBind();
    }
    auto title = aChat.getTitle();
    if (title)
    {
        mEncryptedTitle = title;
    }
    initWithChatd();
}

promise::Promise<void> GroupChatRoom::decryptTitle()
{
    if (mEncryptedTitle.empty())
        return promise::_Void();
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
    return this->chat().crypto()->decryptChatTitle(buf)
    .then([this](const std::string& title)
    {
        if (mTitleString == title)
        {
            KR_LOG_DEBUG("decryptTitle: Same title has been set, skipping update");
            return;
        }
        mTitleString = title;
        mHasTitle = true;
        sqliteQuery(parent.client.db, "update chats set title=? where chatid=?", mTitleString, mChatid);
        if (mRoomGui)
            mRoomGui->onTitleChanged(mTitleString);
        if (mAppChatHandler)
            mAppChatHandler->onTitleChanged(mTitleString);
    })
    .fail([this](const promise::Error& err)
    {
        KR_LOG_ERROR("Error decrypting chat title for chat %s:\n%s\nFalling back to member names.", karere::Id(chatid()).toString().c_str(), err.what());
        makeTitleFromMemberNames();
    });
}

void GroupChatRoom::makeTitleFromMemberNames()
{
    mHasTitle = false;
    mTitleString.clear();
    for (auto& m: mPeers)
    {
        auto& name = m.second->mName;
        if (name.size() <= 1)
            mTitleString.append("...,");
        else
            mTitleString.append(name.c_str()+1, name.size()-1).append(", ");
    }
    if (!mTitleString.empty())
        mTitleString.resize(mTitleString.size()-2); //truncate last ", "

    if (mRoomGui)
        mRoomGui->onTitleChanged(mTitleString);
    if(mAppChatHandler)
        mAppChatHandler->onTitleChanged(mTitleString);
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
    return chat().crypto()->encryptChatTitle(title)
    .then([this](const std::shared_ptr<Buffer>& buf)
    {
        auto b64 = base64urlencode(buf->buf(), buf->dataSize());
        return parent.client.api.call(&::mega::MegaApi::setChatTitle, chatid(),
            b64.c_str());
    })
    .then([this, title](const ReqResult&)
    {
        if (title.empty())
        {
            mHasTitle = false;
            sqliteQuery(parent.client.db, "update chats set title=NULL where chatid=?", mChatid);
            makeTitleFromMemberNames();
        }
    });
}

GroupChatRoom::~GroupChatRoom()
{
    auto chatd = parent.client.chatd.get();
    if (chatd)
        chatd->leave(mChatid);

    for (auto& m: mPeers)
    {
        delete m.second;
    }

    if (mRoomGui)
        parent.client.app.chatListHandler()->removeGroupChatItem(*mRoomGui);
}

promise::Promise<ReqResult> GroupChatRoom::leave()
{
    //rely on actionpacket to do the actual removal of the group
    return parent.client.api.call(&mega::MegaApi::removeFromChat, mChatid, parent.client.myHandle());
}

promise::Promise<void> GroupChatRoom::invite(uint64_t userid, chatd::Priv priv)
{
    promise::Promise<std::string> pms = mHasTitle
        ? chat().crypto()->encryptChatTitle(mTitleString)
          .then([](const std::shared_ptr<Buffer>& buf)
          {
               return base64urlencode(buf->buf(), buf->dataSize());
          })
        : promise::Promise<std::string>(std::string());

    return pms
    .then([this, userid, priv](const std::string& title)
    {
        return parent.client.api.call(&mega::MegaApi::inviteToChat, mChatid, userid, priv,
            title.empty() ? nullptr: title.c_str());
    })
    .then([this, userid, priv](ReqResult)
    {
        mPeers.emplace(userid, new Member(*this, userid, priv));
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
    auto url = chat.getUrl();
    if (url)
    {
        if (strcmp(url, mUrl.c_str()))
        {
            mUrl = url;
            changed = true;
            sqliteQuery(db, "update chats set url=? where chatid=?", mUrl, mChatid);
            KR_LOG_DEBUG("Chatroom %s: URL updated from API", Id(mChatid).toString().c_str());
        }
    }
    chatd::Priv ownPriv = (chatd::Priv)chat.getOwnPrivilege();
    if (ownPriv != mOwnPriv)
    {
        mOwnPriv = ownPriv;
        changed = true;
        sqliteQuery(db, "update chats set own_priv=? where chatid=?", ownPriv, mChatid);
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

Presence PeerChatRoom::presence() const
{
    return (mChat && mChat->onlineState() == chatd::kChatStateOnline)
        ? Presence::kOnline
        : Presence::kOffline;
}

void PeerChatRoom::updatePresence()
{
    auto pres = presence();
    if (mRoomGui)
        mRoomGui->onPresenceChanged(pres);
    if (mAppChatHandler)
        mAppChatHandler->onPresenceChanged(pres);
}

void GroupChatRoom::updateAllOnlineDisplays(Presence pres)
{
    if (mRoomGui)
        mRoomGui->onPresenceChanged(pres);
    if (mAppChatHandler)
        mAppChatHandler->onPresenceChanged(pres);
}

void GroupChatRoom::onUserJoin(Id userid, chatd::Priv privilege)
{
    if (userid == parent.client.myHandle())
        return;
    addMember(userid, privilege, false);
    if (mRoomGui)
        mRoomGui->onUserJoin(userid, privilege);
}

void GroupChatRoom::onUserLeave(Id userid)
{
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

void ChatRoom::onRecvNewMessage(chatd::Idx idx, chatd::Message &msg, chatd::Message::Status status)
{
    auto display = roomGui();
    if (display)
        display->onUnreadCountChanged(mChat->unreadMsgCount());
}
void ChatRoom::onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message &msg)
{
    auto display = roomGui();
    if (display)
        display->onUnreadCountChanged(mChat->unreadMsgCount());
}

void PeerChatRoom::onOnlineStateChange(chatd::ChatState state)
{
    auto pres = mContact.xmppContact().presence();
    mContact.onPresence(pres);
}
void PeerChatRoom::onUnreadChanged()
{
    auto count = mChat->unreadMsgCount();
    if (mRoomGui)
        mRoomGui->onUnreadCountChanged(count);
    if (mContact.appItem())
        mContact.appItem()->onUnreadCountChanged(count);
}

void GroupChatRoom::onOnlineStateChange(chatd::ChatState state)
{
    updateAllOnlineDisplays((state == chatd::kChatStateOnline)
        ? Presence::kOnline
        : Presence::kOffline);
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
            sqliteQuery(db, "delete from chat_peers where chatid=? and userid=?", mChatid, userid);
            KR_LOG_DEBUG("GroupChatRoom[%s]:syncMembers: Removed member %s",
                 Id(mChatid).toString().c_str(),  Id(userid).toString().c_str());
        }
        else
        {
            if (ourIt->second->mPriv != it->second)
            {
                changed = true;
                sqliteQuery(db, "update chat_peers set priv=? where chatid=? and userid=?",
                    it->second, mChatid, userid);
                KR_LOG_DEBUG("GroupChatRoom[%s]:syncMembers: Changed privilege of member %s: %d -> %d",
                     Id(chatid()).toString().c_str(), Id(userid).toString().c_str(),
                     ourIt->second->mPriv, it->second);
                ourIt->second->mPriv = it->second;
            }
            ourIt++;
        }
    }
    for (auto& user: users)
    {
        if (mPeers.find(user.first) == mPeers.end())
        {
            changed = true;
            addMember(user.first, user.second, true);
        }
    }
    return changed;
}

bool GroupChatRoom::syncWithApi(const mega::MegaTextChat& chat)
{
    bool changed = ChatRoom::syncRoomPropertiesWithApi(chat);
    UserPrivMap membs;
    changed |= syncMembers(apiMembersToMap(chat, membs));
    auto title = chat.getTitle();
    if (title)
    {
        mEncryptedTitle = title;
        if (parent.client.contactsLoaded())
        {
            decryptTitle();
        }
    }
    if (changed)
        KR_LOG_DEBUG("Synced group chatroom %s with API.", Id(mChatid).toString().c_str());
    else
        KR_LOG_DEBUG("Sync group chatroom %s with API: no changes", Id(mChatid).toString().c_str());
    return changed;
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
: mRoom(aRoom), mPriv(aPriv)
{
    mNameAttrCbHandle = mRoom.parent.client.userAttrCache().getAttr(user, mega::MegaApi::USER_ATTR_LASTNAME, this,
    [](Buffer* buf, void* userp)
    {
        auto self = static_cast<Member*>(userp);
        if (buf)
        {
            self->mName.assign(buf->buf(), buf->dataSize());
        }
        else
        {
            self->mName = "\x01?";
        }
        if (!self->mRoom.hasTitle())
        {
            self->mRoom.makeTitleFromMemberNames();
        }
    });
}
GroupChatRoom::Member::~Member()
{
    mRoom.parent.client.userAttrCache().removeCb(mNameAttrCbHandle);
}

void Client::connectToChatd()
{
    for (auto& chatItem: *chats)
    {
        chatItem.second->connect();
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
        sqliteQuery(client.db, "update contacts set visibility = ? where userid = ?",
            newVisibility, userid);
        item->onVisibilityChanged(newVisibility);
        return true;
    }
    auto cmail = user.getEmail();
    std::string email(cmail?cmail:"");
    int visibility = user.getVisibility();
    auto ts = user.getTimestamp();
    sqliteQuery(client.db, "insert or replace into contacts(userid, email, visibility, since) values(?,?,?,?)",
            userid, email, visibility, ts);
    item = new Contact(*this, userid, email, visibility, ts, nullptr);
    KR_LOG_DEBUG("Added new user from API: %s", email.c_str());
    return true;
}

void ContactList::syncWithApi(mega::MegaUserList& users)
{
    std::set<uint64_t> apiUsers;
    auto size = users.size();
    for (int i=0; i<size; i++)
    {
        auto& user = *users.get(i);
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
    addUserFromApi(user);
}

void ContactList::removeUser(uint64_t userid)
{
    auto it = find(userid);
    if (it == end())
    {
        KR_LOG_ERROR("ContactList::removeUser: Unknown user");
        return;
    }
    removeUser(it);
}

void ContactList::removeUser(iterator it)
{
    auto handle = it->first;
    delete it->second;
    erase(it);
    sqliteQuery(client.db, "delete from contacts where userid=?", handle);
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

    return api.call(&::mega::MegaApi::removeContact, user.get())
    .then([this, userid](ReqResult ret)->promise::Promise<void>
    {
//we don't remove it, we just set visibility to HIDDEN
//        auto erased = find(userid);
//        if (erased != end())
//            removeUser(erased);
        return promise::_Void();
    });
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

Contact& ContactList::contactFromUserId(uint64_t userid) const
{
    auto it = find(userid);
    if (it == end())
        throw std::runtime_error("contactFromFromUserId: There is no contact with userid "+karere::Id(userid).toString());
    return *it->second;
}

void Client::onContactRequestsUpdate(mega::MegaApi*, mega::MegaContactRequestList* reqs)
{
    if (!reqs)
        return;
    std::shared_ptr<mega::MegaContactRequestList> copy(reqs->copy());
    marshallCall([this, copy]()
    {
        auto count = copy->size();
        for (int i=0; i<count; i++)
        {
            auto& req = *copy->get(i);
            if (req.isOutgoing())
                continue;
            if (req.getStatus() == mega::MegaContactRequest::STATUS_UNRESOLVED)
                app.onIncomingContactRequest(req);
        }
    });
}

Contact::Contact(ContactList& clist, const uint64_t& userid,
                 const std::string& email, int visibility,
                 int64_t since, PeerChatRoom* room)
    :mClist(clist), mUserid(userid), mChatRoom(room), mEmail(email),
     mSince(since), mTitleString(email), mVisibility(visibility)
{
    auto appClist = clist.client.app.contactListHandler();
    mDisplay = appClist ? appClist->addContactItem(*this) : nullptr;
    updateTitle(email);
    mUsernameAttrCbId = mClist.client.userAttrCache().getAttr(userid,
        mega::MegaApi::USER_ATTR_LASTNAME, this,
        [](Buffer* data, void* userp)
        {
            auto self = static_cast<Contact*>(userp);
            if (!data || data->dataSize() < 2)
                self->updateTitle(self->mEmail);
            else
                self->updateTitle(std::string(data->buf()+1, data->dataSize()-1));
        });
    //FIXME: Is this safe? We are passing a virtual interface to 'this' in the ctor
    mXmppContact = mClist.client.xmppContactList().addContact(*this);
}
void Contact::updateTitle(const std::string& str)
{
    mTitleString = str;
    if (mDisplay)
    {
        mDisplay->onTitleChanged(str);
    }
    if (mChatRoom)
    {
        auto display = mChatRoom->roomGui();
        if (display)
            display->onTitleChanged(str);
        if (mChatRoom->appChatHandler())
            mChatRoom->appChatHandler()->onTitleChanged(str);
    }
}

Contact::~Contact()
{
    mClist.client.userAttrCache().removeCb(mUsernameAttrCbId);
    if (mXmppContact)
        mXmppContact->setPresenceListener(nullptr);

    if (mDisplay)
        mClist.client.app.contactListHandler()->removeContactItem(*mDisplay);
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
        auto& room = mClist.client.chats->addRoom(*list.get(0));
        return &room;
    });
}

void Contact::setChatRoom(PeerChatRoom& room)
{
    assert(!mChatRoom);
    mChatRoom = &room;
    auto display = room.roomGui();
    if (display)
        display->onTitleChanged(mTitleString);
    if (room.appChatHandler())
        room.appChatHandler()->onTitleChanged(mTitleString);
}

void Contact::attachChatRoom(PeerChatRoom& room)
{
    if (mChatRoom)
        throw std::runtime_error("attachChatRoom[room "+Id(room.chatid()).toString()+ "]: contact "+
            Id(mUserid).toString()+" already has a chat room attached");
    CHAT_LOG_DEBUG("Attaching 1on1 chatroom %s to contact %s", Id(room.chatid()).toString().c_str(), Id(mUserid).toString().c_str());
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

void Client::discoAddFeature(const char *feature)
{
    conn->plugin<disco::DiscoPlugin>("disco").addFeature(feature);
}

rtcModule::IEventHandler* Client::onIncomingCallRequest(
        const std::shared_ptr<rtcModule::ICallAnswer> &ans)
{
    return app.onIncomingCall(ans);
}

}
