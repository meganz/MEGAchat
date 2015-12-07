#include "contactList.h"
#include "ITypes.h" //for IPtr
#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mstrophepp.h>
#include "rtcModule/IRtcModule.h"
#include "rtcModule/lib.h"
#include "dummyCrypto.h" //for makeRandomString
#include "strophe.disco.h"
#include "base/services.h"
#include "sdkApi.h"
#include "megaCryptoFunctions.h"
#include <serverListProvider.h>
#include <memory>
#include "chatClient.h"
#include "textModule.h"
#include <chatd.h>
#include <db.h>
#include <buffer.h>

#define _QUICK_LOGIN_NO_RTC

namespace karere
{

promise::Promise<int> Client::initializeContactList()
{
    strophe::Stanza roster(*conn);
    roster.setName("iq")
          .setAttr("type", "get")
          .setAttr("from", conn->fullJid())
          .c("query")
              .setAttr("xmlns", "jabber:iq:roster");
    return conn->sendIqQuery(roster, "roster")
    .then([this](strophe::Stanza s) mutable
    {
        auto query = s.child("query", true);
        if (query)
        {
            query.forEachChild("item", [this](strophe::Stanza c)
            {
                const char* attrVal = c.attrOrNull("jid");
                if (!attrVal)
                    return;

                contactList.addContact(std::string(attrVal));
                message_bus::SharedMessage<> busMessage(CONTACT_ADDED_EVENT);
                busMessage->addValue(CONTACT_JID, std::string(attrVal));
                message_bus::SharedMessageBus<>::getMessageBus()->alertListeners(CONTACT_ADDED_EVENT, busMessage);
            });
        }
        return contactList.init();
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_WARNING("Error receiving contact list");
        return err;
    });
}


void Client::sendPong(const std::string& peerJid, const std::string& messageId)
{
    strophe::Stanza pong(*conn);
    pong.setAttr("type", "result")
        .setAttr("to", peerJid)
        .setAttr("from", conn->fullJid())
        .setAttr("id", messageId);

    conn->send(pong);
}


Client::Client(const std::string& email, const std::string& password)
 :conn(new strophe::Connection(services_strophe_get_ctx())),
  api(new MyMegaApi("karere-native")),mEmail(email), mPassword(password),
  contactList(conn),
  mXmppServerProvider(new XmppServerProvider("https://gelb530n001.karere.mega.nz", "xmpp", KARERE_FALLBACK_XMPP_SERVERS)),
  mRtcHandler(NULL)
{
    const char* homedir = getenv("HOME");
    if (!homedir)
        throw std::runtime_error("Cant get HOME env variable");

    std::string path(homedir);
    path.append("/.karere.db");
    int ret = sqlite3_open(path.c_str(), &db);
    if (ret != SQLITE_OK)
    {
        db = nullptr;
        throw std::runtime_error("Can't access application database at "+path);
    }
    userAttrCache.reset(new UserAttrCache(*this));
}


Client::~Client()
{
    //when the strophe::Connection is destroyed, its handlers are automatically destroyed
}


void Client::registerRtcHandler(rtcModule::IEventHandler* rtcHandler)
{
    mRtcHandler.reset(rtcHandler);
}
#define TOKENPASTE2(a,b) a##b
#define TOKENPASTE(a,b) TOKENPASTE2(a,b)

#define SHARED_STATE(varname, membtype)             \
    struct TOKENPASTE(SharedState,__LINE__){membtype value;};  \
    std::shared_ptr<TOKENPASTE(SharedState, __LINE__)> varname(new TOKENPASTE(SharedState,__LINE__))

promise::Promise<int> Client::init()
{
    SqliteStmt stmt(db, "select value from vars where name='lastsid'");
    std::string sid = stmt.step() ? stmt.stringCol(0) : std::string();
    auto pmsMegaLogin = sid.empty()
            ? api->call(&mega::MegaApi::login, mEmail.c_str(), mPassword.c_str())
            : api->call(&mega::MegaApi::fastLogin, sid.c_str());

    pmsMegaLogin.then([this](ReqResult result) mutable
    {
        KR_LOG_DEBUG("Login to Mega API successful");
        SdkString uh = api->getMyUserHandle();
        if (!uh.c_str() || !uh.c_str()[0])
            throw std::runtime_error("Could not get our own userhandle/JID");
        mMyUserHandle = uh.c_str();
        mChatd.reset(new chatd::Client(mMyUserHandle.c_str(), 0));
//        if (onChatdReady)
//            onChatdReady();

        SdkString xmppPass = api->dumpXMPPSession();
        if (xmppPass.size() < 16)
            throw std::runtime_error("Mega session id is shorter than 16 bytes");
        ((char&)xmppPass.c_str()[16]) = 0;

        //xmpp_conn_set_keepalive(*conn, 10, 4);
        // setup authentication information
        std::string jid = std::string(api->getMyXMPPJid())+"@" KARERE_XMPP_DOMAIN "/kn_";
        jid.append(rtcModule::makeRandomString(10));
        xmpp_conn_set_jid(*conn, jid.c_str());
        xmpp_conn_set_pass(*conn, xmppPass.c_str());
        KR_LOG_DEBUG("xmpp user = '%s', pass = '%s'", jid.c_str(), xmppPass.c_str());
        setupHandlers();
    });
    auto pmsMegaGud = pmsMegaLogin.then([this](ReqResult result)
    {
        return api->call(&mega::MegaApi::getUserData);
    })
    .then([this](ReqResult result)
    {
        api->userData = result;
    });
    auto pmsMegaFetch = pmsMegaLogin.then([this](ReqResult result)
    {
        return api->call(&mega::MegaApi::fetchNodes);
    });

    SHARED_STATE(server, std::shared_ptr<HostPortServerInfo>);
    auto pmsGelbReq = mXmppServerProvider->getServer()
    .then([server](std::shared_ptr<HostPortServerInfo> aServer) mutable
    {
        server->value = aServer;
        return 0;
    });
    return promise::when(pmsMegaGud, pmsMegaFetch, pmsGelbReq)
    .then([this, server]()
    {
        if (onChatdReady)
            onChatdReady();

// initiate connection
        return mega::retry([this, server](int no) -> promise::Promise<void>
        {
            if (no < 2)
            {
                return mega::performWithTimeout([this, server]()
                {
                    KR_LOG_INFO("Connecting to xmpp server %s...", server->value->host.c_str());
                    return conn->connect(server->value->host.c_str(), 0);
                }, KARERE_LOGIN_TIMEOUT,
                [this]()
                {
                    xmpp_disconnect(*conn, -1);
                });
            }
            else
            {
                return mXmppServerProvider->getServer()
                .then([this](std::shared_ptr<HostPortServerInfo> aServer)
                {
                    KR_LOG_WARNING("Connecting to new xmpp server: %s...", aServer->host.c_str());
                    return mega::performWithTimeout([this, aServer]()
                    {
                        return conn->connect(aServer->host.c_str(), 0);
                    }, KARERE_LOGIN_TIMEOUT,
                    [this]()
                    {
                        xmpp_disconnect(*conn, -1);
                    });
                });
            }
        }, nullptr, 0, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL);
    })
    .fail([](const promise::Error& error)
    {
        KR_LOG_ERROR("XMPP login error:\n%s", error.what());
        return error;
    })
    .then([this]()
    {
        KR_LOG_INFO("XMPP login success");

// handle reconnect due to network errors
        setupReconnectHandler();

// Create and register the rtcmodule plugin
// the MegaCryptoFuncs object needs api->userData (to initialize the private key etc)
// To use DummyCrypto: new rtcModule::DummyCrypto(jid.c_str());
        rtc = createRtcModule(*conn, mRtcHandler.get(), new rtcModule::MegaCryptoFuncs(*api), KARERE_DEFAULT_TURN_SERVERS);
        conn->registerPlugin("rtcmodule", rtc);
// create and register text chat plugin
        mTextModule = new TextModule(*this);
        conn->registerPlugin("textchat", mTextModule);
// create and register disco strophe plugin
        conn->registerPlugin("disco", new disco::DiscoPlugin(*conn, "Karere Native"));
        KR_LOG_DEBUG("webrtc and textchat plugins initialized");
// install contactlist handlers before sending initial presence so that the presences that start coming after that get processed
        auto pms = initializeContactList();
// Send initial <presence/> so that we appear online to contacts
        strophe::Stanza pres(*conn);
        pres.setName("presence");
        conn->send(pres);
        return pms;
    })
    .then([this](int)
    {
        KR_LOG_DEBUG("Contactlist initialized");
        //startKeepalivePings();
        return 0;
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error initializing client: %s", err.what());
        return err;
    });
}

void Client::setupHandlers()
{
    conn->addHandler([this](strophe::Stanza stanza, void*, bool &keep) mutable
    {
            sendPong(stanza.attr("from"), stanza.attr("id"));
    }, "urn::xmpp::ping", "iq", nullptr, nullptr);

}

void Client::setupReconnectHandler()
{
    mReconnectController.reset(mega::createRetryController(
    [this](int no)
    {
        mLastPingTs = 0;
        return conn->connect(KARERE_DEFAULT_XMPP_SERVER, 0);
    },
    [this]()
    {
        xmpp_disconnect(*conn, -1);
    }, KARERE_LOGIN_TIMEOUT, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL));

    mReconnectConnStateHandler = conn->addConnStateHandler(
       [this](xmpp_conn_event_t event, int error,
        xmpp_stream_error_t* stream_error, bool& keepHandler) mutable
    {
        if (((event != XMPP_CONN_DISCONNECT) && (event != XMPP_CONN_FAIL)) || isTerminating)
            return;
        assert(xmpp_conn_get_state(*conn) == XMPP_STATE_DISCONNECTED);
        if (mReconnectController->state() & mega::rh::kStateBitRunning)
            return;

        if (mReconnectController->state() == mega::rh::kStateFinished) //we had previous retry session, reset the retry controller
            mReconnectController->reset();
        mReconnectController->start(500); //need to process(i.e. ignore) all stale libevent messages for the old connection so they don't get interpreted in the context of the new connection
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
        assert(mReconnectController->state() != mega::rh::kStateFinished);
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

    if (mReconnectController->state() == mega::rh::kStateFinished)
    {
        KR_LOG_WARNING("notifyNetworkOnline: reconnect controller is in 'finished' state, but connection is not connected. Resetting reconnect controller.");
        mReconnectController->reset();
    }
    mReconnectController->restart();
}

promise::Promise<void> Client::terminate()
{
    if (isTerminating)
    {
        KR_LOG_WARNING("Client::terminate: Already terminating");
        return promise::Promise<void>();
    }
    isTerminating = true;
    if (mReconnectConnStateHandler)
    {
        conn->removeConnStateHandler(mReconnectConnStateHandler);
        mReconnectConnStateHandler = 0;
    }
    if (mReconnectController)
        mReconnectController->abort();
    if (rtc)
        rtc->hangupAll("app-close", "The application is terminating");
    const char* sess = api->dumpSession();
    if (sess)
    try
    {
        SqliteStmt stmt(db, "insert or replace into vars(name,value) values('lastsid',?2)");
        stmt << "lastsid" << sess;
        stmt.step();
    }
    catch(std::exception& e)
    {
        KR_LOG_ERROR("Error while saving session id to database: %s", e.what());
    }
    sqlite3_close(db);
    promise::Promise<void> pms;
    conn->disconnect(2000)
    //resolve output promise asynchronously, because the callbacks of the output
    //promise may free the client, and the resolve()-s of the input promises
    //(mega and conn) are within the client's code, so any code after the resolve()s
    //that tries to access the client will crash
    .then([pms](int) mutable
    {
        mega::marshallCall([pms]() mutable { pms.resolve(); });
    })
    .fail([pms](const promise::Error& err) mutable
    {
        mega::marshallCall([pms, err]() mutable { pms.reject(err); });
        return err;
    });
    return pms;
}

void Client::startKeepalivePings()
{
    mega::setInterval([this]()
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

void Client::setPresence(const Presence pres, const int delay)
{
    strophe::Stanza msg(*conn);
    msg.setName("presence")
       .setAttr("id", generateMessageId(std::string("presence"), std::string("")))
       .c("show")
           .t(ContactList::presenceToText(pres))
           .up()
       .c("status")
           .t(ContactList::presenceToText(pres))
           .up();

    if(delay > 0)
    {
        msg.c("delay")
                .setAttr("xmlns", "urn:xmpp:delay")
                .setAttr("from", conn->fullJid());
    }
    conn->send(msg);
}


promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
Client::getOtherUserInfo(std::string &emailAddress)
{
    std::string event(USER_INFO_EVENT);
    event.append(emailAddress);
    message_bus::SharedMessage<M_MESS_PARAMS> userMessage(event);

    return api->call(&mega::MegaApi::getUserData, emailAddress.c_str())
    .then([this, userMessage](ReqResult result)
    {
        //const char *peer = result->getText();
        //const char *pk = result->getPassword();

        return userMessage;
    });
/*
 * //av: cant return nullptr instead of SharedMessage. This works only for a class with constructor that can take nullptr as argument, and the ctor is not marked as explicit
   .fail([&](const promise::Error &err)
    {
        return nullptr;
    });
*/
}
UserAttrDesc attrDesc[kUserAttrLast+1] =
{
   { 0,0 } //username is special, so we don't use a descriptor for it
};
UserAttrCache::~UserAttrCache()
{}

void UserAttrCache::dbWrite(const UserAttrPair& key, const Buffer& data)
{
    SqliteStmt stmt(mClient.db, "insert or replace into userattrs(userid, type, data) values(?,?,?)");
    stmt << key.user << key.attrType << data;
    stmt.step();
}

UserAttrCache::UserAttrCache(Client& aClient): mClient(aClient)
{
    SqliteStmt stmt(mClient.db, "select userid, type, data from userattrs");
    while(stmt.step())
    {
        std::unique_ptr<Buffer> data(new Buffer((size_t)sqlite3_column_bytes(stmt, 2)));
        stmt.blobCol(2, *data);

        emplace(std::piecewise_construct,
                std::forward_as_tuple(stmt.uint64Col(0), stmt.intCol(1)),
                std::forward_as_tuple(std::make_shared<UserAttrCacheItem>(data.release(), false)));
    }
}

void UserAttrCacheItem::notify()
{
    for (auto it=cbs.begin(); it!=cbs.end();)
    {
        auto curr = it;
        it++;
        curr->cb(data, curr->userp); //may erase curr
    }
}
UserAttrCacheItem::~UserAttrCacheItem()
{
    if (data)
        delete data;
}

uint64_t UserAttrCache::addCb(iterator itemit, UserAttrReqCbFunc cb, void* userp)
{
    auto& cbs = itemit->second->cbs;
    auto it = cbs.emplace(cbs.end(), cb, userp);
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
            void* userp, UserAttrReqCbFunc cb)
{
    if (type > kUserAttrLast)
        throw std::runtime_error("Invalid attribute id specified");
    UserAttrPair key(userHandle, type);
    auto it = find(key);
    if (it != end())
    {
        auto& item = *it->second;
        if (cb)
        { //TODO: not optimal to store each cb pointer, as these pointers would be mostly only a few, with different userp-s
            auto cbid = addCb(it, cb, userp);
            if (!item.pending)
                cb(item.data, userp);
            return cbid;
        }
        else
        {
            return 0;
        }
    }

    auto item = std::make_shared<UserAttrCacheItem>(nullptr, true);
    it = emplace(key, item).first;
    uint64_t cbid = cb ? addCb(it, cb, userp) : 0;

    if (type != kUserAttrName)
    {
        auto& attrType = attrDesc[type];
        mClient.api->call(&mega::MegaApi::getUserAttribute,
            chatd::base64urlencode(&userHandle, sizeof(userHandle)).c_str(), attrType.sdkId)
        .then([this, &attrType, key, item](ReqResult result)
        {
            item->pending = false;
            item->data = attrType.getData(*result);
            dbWrite(key, *item->data);
            item->notify();
        })
        .fail([this, item](const promise::Error& err)
        {
            item->pending = false;
            item->data = nullptr;
            item->notify();
            return err;
        });
    }
    else
    {
        item->data = new Buffer;
        std::string strUh = chatd::base64urlencode(&userHandle, sizeof(userHandle));
        mClient.api->call(&mega::MegaApi::getUserAttribute, strUh.c_str(),
                  (int)MyMegaApi::USER_ATTR_FIRSTNAME)
        .then([this, strUh, key, item](ReqResult result)
        {
            const char* name = result->getText();
            item->data->append(name?name:"(null)");
            return mClient.api->call(&mega::MegaApi::getUserAttribute, strUh.c_str(),
                    (int)MyMegaApi::USER_ATTR_LASTNAME);
        })
        .then([this, key, item](ReqResult result)
        {
            Buffer* data = item->data;
            data->append(' ');
            const char* name = result->getText();
            data->append(name ? name : "(null)").append<char>(0);
            item->pending = false;
            dbWrite(key, *data);
            item->notify();
        })
        .fail([this, item](const promise::Error& err)
        {
            item->data = nullptr;
            item->pending = false;
            item->notify();
            return err;
        });
    }
    return cbid;
}

promise::Promise<message_bus::SharedMessage<M_MESS_PARAMS>>
Client::getThisUserInfo()
{
    std::string event(THIS_USER_INFO_EVENT);
    message_bus::SharedMessage<M_MESS_PARAMS> userMessage(event);

    return api->call(&mega::MegaApi::getUserData)
    .then([this, userMessage](ReqResult result)
    {
        return userMessage; //av: was nullptr, but compile error - Promise<return type of this> must match function return type
    })
    .fail([&](const promise::Error &err)
    {
        return userMessage; //av: same here - was nullptr but compile error
    });
}

}
