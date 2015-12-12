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
#include <chatdDb.h>

#define _QUICK_LOGIN_NO_RTC
using namespace promise;
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


Client::Client(IGui& aGui, const std::string& email, const std::string& password)
 :db(openDb()), conn(new strophe::Connection(services_strophe_get_ctx())),
  api(new MyMegaApi("karere-native")), userAttrCache(*this), gui(aGui),
  mEmail(email), mPassword(password), contactList(conn),
  mXmppServerProvider(new XmppServerProvider("https://gelb530n001.karere.mega.nz", "xmpp", KARERE_FALLBACK_XMPP_SERVERS)),
  mRtcHandler(NULL)
{
}
sqlite3* Client::openDb()
{
    const char* homedir = getenv("HOME");
    if (!homedir)
        throw std::runtime_error("Cant get HOME env variable");

    std::string path(homedir);
    path.append("/.karere.db");
    sqlite3* database = nullptr;
    int ret = sqlite3_open(path.c_str(), &database);
    if (ret != SQLITE_OK || !database)
    {
        throw std::runtime_error("Can't access application database at "+path);
    }
    return database;
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
        chatd.reset(new chatd::Client(mMyUserHandle.c_str(), 0));
        chats.reset(new ChatRoomList(*this));
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
    })
    .then([this](ReqResult result)
    {
//        mContacts.reset(api->getContacts());
        return api->call(&mega::MegaApi::fetchChats);
    })
    .then([this](ReqResult result)
    {
        auto chatRooms = result->getMegaTextChatList();
        if (chatRooms)
        {
            chats->syncRoomsWithApi(*chatRooms);
        }
        else
        {
            printf("NO CHATROOMS FROM API\n");
        }
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
    .then([this]()
    {
        KR_LOG_INFO("Login success");

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
    .then([this, pms](int) mutable
    {
        return api->call(&::mega::MegaApi::localLogout);
    })
    .then([pms](ReqResult result)
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
UserAttrDesc attrDesc[mega::MegaApi::USER_ATTR_LAST_INTERACTION+1] =
{ //getData func | changeMask
  //0 - avatar
   { [](const mega::MegaRequest& req)->Buffer* { return new Buffer(req.getFile(), strlen(req.getFile())); }, mega::MegaUser::CHANGE_TYPE_AVATAR},
  //firstname and lastname are handled specially, so we don't use a descriptor for it
  //1 - first name
   { [](const mega::MegaRequest& req)->Buffer* { return new Buffer(req.getText(), strlen(req.getText())); }, mega::MegaUser::CHANGE_TYPE_FIRSTNAME},
  //2 = last name
   { [](const mega::MegaRequest& req)->Buffer* { return new Buffer(req.getText(), strlen(req.getText())); }, mega::MegaUser::CHANGE_TYPE_LASTNAME},
  //keyring
   { [](const mega::MegaRequest& req)->Buffer* { throw std::runtime_error("not implemented"); }, mega::MegaUser::CHANGE_TYPE_AUTH},
  //last interaction
   { [](const mega::MegaRequest& req)->Buffer* { throw std::runtime_error("not implemented"); }, mega::MegaUser::CHANGE_TYPE_LSTINT}
};

UserAttrCache::~UserAttrCache()
{
    mClient.api->removeGlobalListener(this);
}

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
    mClient.api->addGlobalListener(this);
}

void UserAttrCache::onUsersUpdate(mega::MegaApi* api, mega::MegaUserList *users)
{
    if (!users)
        return;

    for (auto i=0; i<users->size(); i++)
    {
        auto user = users->get(i);
        printf("user %llu change: %d\n", user->getHandle(), user->getChanges());
        int changed = user->getChanges();
        for (auto t = 0; t <= mega::MegaApi::USER_ATTR_LAST_INTERACTION; t++)
        {
            printf("changed = %d, changeMask = %d\n", changed, attrDesc[t].changeMask);
            if ((changed & attrDesc[t].changeMask) == 0)
                continue;
            UserAttrPair key(user->getHandle(), t);
            auto it = find(key);
            if (it == end()) //we don't have such attribute
                continue;
            auto& item = it->second;
            if (item->pending)
                continue;
            item->pending = kCacheFetchUpdatePending;
            fetchAttr(key, item);
        }
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
    UserAttrPair key(userHandle, type);
    auto it = find(key);
    if (it != end())
    {
        auto& item = *it->second;
        if (cb)
        { //TODO: not optimal to store each cb pointer, as these pointers would be mostly only a few, with different userp-s
            auto cbid = addCb(it, cb, userp);
            if (item.pending != kCacheFetchNewPending)
                cb(item.data, userp);
            return cbid;
        }
        else
        {
            return 0;
        }
    }

    auto item = std::make_shared<UserAttrCacheItem>(nullptr, kCacheFetchNewPending);
    it = emplace(key, item).first;
    uint64_t cbid = cb ? addCb(it, cb, userp) : 0;
    fetchAttr(key, item);
    return cbid;
}
void UserAttrCache::fetchAttr(const UserAttrPair& key, std::shared_ptr<UserAttrCacheItem>& item)
{
    if (key.attrType != mega::MegaApi::USER_ATTR_LASTNAME)
    {
        auto& attrType = attrDesc[key.attrType];
        mClient.api->call(&mega::MegaApi::getUserAttribute,
            chatd::base64urlencode(&key.user, sizeof(key.user)).c_str(), (int)key.attrType)
        .then([this, &attrType, key, item](ReqResult result)
        {
            item->pending = kCacheFetchNotPending;
            item->data = attrType.getData(*result);
            dbWrite(key, *item->data);
            item->notify();
        })
        .fail([this, item](const promise::Error& err)
        {
            item->pending = kCacheFetchNotPending;
            item->data = nullptr;
            item->notify();
            return err;
        });
    }
    else
    {
        item->data = new Buffer;
        std::string strUh = chatd::base64urlencode(&key.user, sizeof(key.user));
        mClient.api->call(&mega::MegaApi::getUserAttribute, strUh.c_str(),
                  (int)MyMegaApi::USER_ATTR_FIRSTNAME)
        .then([this, strUh, key, item](ReqResult result)
        {
            const char* name = result->getText();
            if (!name)
                name = "(null)";
            size_t len = strlen(name);
            if (len > 255)
            {
                item->data->append<unsigned char>(255);
                item->data->append(name, 252);
                item->data->append("...", 3);
            }
            else
            {
                item->data->append<unsigned char>(len);
                item->data->append(name);
            }
            return mClient.api->call(&mega::MegaApi::getUserAttribute, strUh.c_str(),
                    (int)MyMegaApi::USER_ATTR_LASTNAME);
        })
        .then([this, key, item](ReqResult result)
        {
            Buffer* data = item->data;
            data->append(' ');
            const char* name = result->getText();
            data->append(name ? name : "(null)").append<char>(0);
            item->pending = kCacheFetchNotPending;
            dbWrite(key, *data);
            item->notify();
        })
        .fail([this, item](const promise::Error& err)
        {
            item->data = nullptr;
            item->pending = kCacheFetchNotPending;
            item->notify();
            return err;
        });
    }
}
promise::Promise<Buffer*> UserAttrCache::getAttr(const uint64_t &user, unsigned attrType)
{
    struct State
    {
        Promise<Buffer*> pms;
        UserAttrCache* self;
        uint64_t cbid;
    };
    State* state = new State;
    state->self = this;
    state->cbid = getAttr(user, attrType, state, [](Buffer* buf, void* userp)
    {
        auto s = static_cast<State*>(userp);
        s->self->removeCb(s->cbid);
        if (buf)
            s->pms.resolve(buf);
        else
            s->pms.reject("failed");
        delete s;
    });
    return state->pms;
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

ChatRoom::ChatRoom(ChatRoomList& parent, const uint64_t& chatid, bool aIsGroup, const std::string& aUrl, unsigned char aShard,
  char aOwnPriv)
:mParent(parent), mChatid(chatid), mUrl(aUrl), mShardNo(aShard), mIsGroup(aIsGroup), mOwnPriv(aOwnPriv),
 mTitleDisplay(mParent.client.gui.createTitleDisplay(*this))
{
    parent.client.chatd->join(mChatid, mShardNo, mUrl, *this);
}

GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl, unsigned char aShard,
    char aOwnPriv, const std::string& title)
:ChatRoom(parent, chatid, true, aUrl, aShard, aOwnPriv), mTitleString(title)
{
    SqliteStmt stmt(parent.client.db, "select user, priv from chat_peers where chatid=?");
    stmt << mChatid;
    while(stmt.step())
    {
        addMember(stmt.uint64Col(0), stmt.intCol(1), false);
    }
}
PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const uint64_t& chatid, const std::string& aUrl,
    unsigned char aShard, char aOwnPriv, const uint64_t& peer, char peerPriv)
:ChatRoom(parent, chatid, false, aUrl, aShard, aOwnPriv), mPeer(peer), mPeerPriv(peerPriv)
{
}

PeerChatRoom::PeerChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat)
:PeerChatRoom(parent, chat.getHandle(), chat.getUrl(), chat.getShard(), chat.getOwnPrivilege(),
              -1, 0)
{
    assert(!chat.isGroup());
    auto peers = chat.getPeerList();
    assert(peers);
    assert(peers->size() == 1);
    mPeer = peers->getPeerHandle(0);
    mPeerPriv = peers->getPeerPrivilege(0);

    sqliteQuery(mParent.client.db, "insert into chats(chatid, url, shard, peer, peer_priv, own_priv) values (?,?,?,?,?,?)",
        mChatid, mUrl, mShardNo, mPeer, mPeerPriv, mOwnPriv);
//just in case
    SqliteStmt stmt(mParent.client.db, "delete from chat_peers where chatid = ?");
    stmt << mChatid;
    stmt.step();
}

void PeerChatRoom::syncOwnPriv(char priv)
{
    if (mOwnPriv == priv)
        return;

    mOwnPriv = priv;
    sqliteQuery(mParent.client.db, "update chats set own_priv = ? where chatid = ?",
                priv, mChatid);
}

void PeerChatRoom::syncPeerPriv(char priv)
{
    if (mPeerPriv == priv)
        return;
    mPeerPriv = priv;
    sqliteQuery(mParent.client.db, "update chats set peer_priv = ? where chatid = ?",
                priv, mChatid);
}
void PeerChatRoom::syncWithApi(const mega::MegaTextChat &chat)
{
    ChatRoom::syncRoomPropertiesWithApi(chat);
    syncOwnPriv(chat.getOwnPrivilege());
    syncPeerPriv(chat.getPeerList()->getPeerPrivilege(0));
}

void GroupChatRoom::addMember(const uint64_t& userid, char priv, bool saveToDb)
{
    auto& m = mPeers[userid];
    if (m)
    {
        if (m->mPriv == priv)
        {
            saveToDb = false;
        }
        else
        {
            m->mPriv = priv;
        }
    }
    else
    {
        m = new Member(*this, userid, priv); //usernames will be updated when the Member object gets the username attribute
    }
    if (saveToDb)
    {
        sqliteQuery(mParent.client.db, "insert or replace into chat_peers(chatid, userid, priv) values(?,?,?)",
            mChatid, userid, priv);
    }
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
    sqliteQuery(mParent.client.db, "delete from chat_peers where chatid=? and userid=?",
                mChatid, userid);
    updateTitle();
    return true;
}

void GroupChatRoom::deleteSelf()
{
    auto db = mParent.client.db;
    sqliteQuery(db, "delete from chat_peers where chatid=?", mChatid);
    sqliteQuery(db, "delete from chats where chatid=?", mChatid);
    delete this;
}

ChatRoomList::ChatRoomList(Client& aClient)
:client(aClient)
{
    loadFromDb();
}

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
        auto peer = stmt.uint64Col(4);
        ChatRoom* room;
        if (peer != uint64_t(-1))
            room = new PeerChatRoom(*this, chatid, stmt.stringCol(1), stmt.intCol(2), stmt.intCol(3), peer, stmt.intCol(5));
        else
            room = new GroupChatRoom(*this, chatid, stmt.stringCol(1), stmt.intCol(2), stmt.intCol(3), stmt.stringCol(6));
        emplace(chatid, room);
    }
}
void ChatRoomList::syncRoomsWithApi(const mega::MegaTextChatList& rooms)
{
    auto size = rooms.size();
    for (int i=0; i<size; i++)
    {
        addRoom(*rooms.get(i));
    }
}
bool ChatRoomList::addRoom(const mega::MegaTextChat& room)
{
    auto chatid = room.getHandle();
    auto it = find(chatid);
    if (it != end()) //we already have that room
    {
        it->second->syncWithApi(room);
        return false;
    }
    if (room.isGroup())
        emplace(chatid, new GroupChatRoom(*this, room)); //also writes it to cache
    else
        emplace(chatid, new PeerChatRoom(*this, room));
    return true;
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

ChatRoomList::~ChatRoomList()
{
    for (auto& room: *this)
        delete room.second;
}

GroupChatRoom::GroupChatRoom(ChatRoomList& parent, const mega::MegaTextChat& chat)
:ChatRoom(parent, chat.getHandle(), true, chat.getUrl(), chat.getShard(), chat.getOwnPrivilege())
{
    auto peers = chat.getPeerList();
    assert(peers);
    auto size = peers->size();
    for (int i=0; i<size; i++)
    {
        auto handle = peers->getPeerHandle(i);
        mPeers[handle] = new Member(*this, handle, peers->getPeerPrivilege(i));
    }
//save to db
    auto db = karere::gClient->db;
    sqliteQuery(db, "delete from chat_peers where chatid=?", mChatid);
    sqliteQuery(db, "insert or replace into chats(chatid, url, shard, peer, peer_priv, own_priv) values(?,?,?,-1,0,?)",
                mChatid, mUrl, mShardNo, mOwnPriv);

    SqliteStmt stmt(db, "insert into chat_peers(chatid, user, priv) values(?,?,?)");
    for (auto& m: mPeers)
    {
        stmt << mChatid << m.first << m.second->mPriv;
        stmt.step();
        stmt.reset();
    }
    loadUserTitle();
}

void GroupChatRoom::loadUserTitle()
{
    //load user title if set
    SqliteStmt stmt(mParent.client.db, "select title from chats where chatid = ?");
    stmt << mChatid;
    if (!stmt.step())
    {
        mHasUserTitle = false;
        return;
    }
    std::string strTitle = stmt.stringCol(0);
    if (strTitle.empty())
    {
        mHasUserTitle = false;
        return;
    }
    mTitleString = strTitle;
    mHasUserTitle = true;
}

void ChatRoom::syncRoomPropertiesWithApi(const mega::MegaTextChat &chat)
{
    if (chat.getShard() != mShardNo)
        throw std::runtime_error("syncWithApi: Shard number of chat can't change");
    if (chat.isGroup() != mIsGroup)
        throw std::runtime_error("syncWithApi: isGroup flag can't change");
    auto db = karere::gClient->db;
    auto url = chat.getUrl();
    if (!url)
        throw std::runtime_error("MegaTextChat::getUrl() returned NULL");
    if (strcmp(url, mUrl.c_str()))
    {
        mUrl = url;
        sqliteQuery(db, "update chats set url=? where chatid=?", mUrl, mChatid);
    }
    char ownPriv = chat.getOwnPrivilege();
    if (ownPriv != mOwnPriv)
    {
        mOwnPriv = ownPriv;
        sqliteQuery(db, "update chats set own_priv=? where chatid=?", ownPriv, mChatid);
    }
}
void ChatRoom::init(chatd::Messages* msgs, chatd::DbInterface*& dbIntf)
{
    mMessages = msgs;
    dbIntf = new ChatdSqliteDb(msgs, karere::gClient->db);
    if (mChatWindow)
    {
        chatd::DbInterface* nullIntf = nullptr;
        mChatWindow->init(msgs, nullIntf);
    }
}
void GroupChatRoom::onUserJoined(const chatd::Id &userid, char privilege)
{
    addMember(userid, privilege, true);
}
void GroupChatRoom::onUserLeft(const chatd::Id &userid)
{
    removeMember(userid);
}

void PeerChatRoom::onUserJoined(const chatd::Id &userid, char privilege)
{
    if (userid == mParent.client.chatd->userId())
        syncOwnPriv(privilege);
    else if (userid.val == mPeer)
        syncPeerPriv(privilege);
    else
        KR_LOG_ERROR("PeerChatRoom: Bug: Received JOIN event from chatd for a third user, ignoring");
}

void PeerChatRoom::onUserLeft(const chatd::Id &userid)
{
    KR_LOG_ERROR("PeerChatRoom: Bug: Received an user leave event from chatd on a permanent chat, ignoring");
}
void ChatRoom::onRecvNewMessage(chatd::Idx idx, chatd::Message &msg, chatd::Message::Status status)
{
    printf("updating overlay count to %u\n", mMessages->unreadMsgCount());
    mTitleDisplay->updateOverlayCount(mMessages->unreadMsgCount());
}
void ChatRoom::onMessageStatusChange(chatd::Idx idx, chatd::Message::Status newStatus, const chatd::Message &msg)
{
    mTitleDisplay->updateOverlayCount(mMessages->unreadMsgCount());
}

void GroupChatRoom::syncMembers(const chatd::UserPrivMap& users)
{
    auto db = karere::gClient->db;
    for (auto ourIt=mPeers.begin(); ourIt!=mPeers.end();)
    {
        auto userid = ourIt->first;
        auto it = users.find(userid);
        if (it == users.end()) //we have a user that is not in the chatroom anymore
        {
            auto erased = ourIt;
            ourIt++;
            auto member = erased->second;
            mPeers.erase(erased);
            delete member;
            sqliteQuery(db, "delete from chat_peers where chatid=? and userid=?", mChatid, userid);
        }
        else
        {
            if (ourIt->second->mPriv != it->second)
            {
                sqliteQuery(db, "update chat_peers where chatid=? and userid=? set priv=?",
                    mChatid, userid, it->second);
            }
            ourIt++;
        }
    }
    for (auto& user: users)
    {
        if (mPeers.find(user.first) == mPeers.end())
            addMember(user.first, user.second, true);
    }
}

void GroupChatRoom::syncWithApi(const mega::MegaTextChat& chat)
{
    ChatRoom::syncRoomPropertiesWithApi(chat);
    chatd::UserPrivMap membs;
    syncMembers(apiMembersToMap(chat, membs));
}


chatd::UserPrivMap& GroupChatRoom::apiMembersToMap(const mega::MegaTextChat& chat, chatd::UserPrivMap& membs)
{
    auto members = chat.getPeerList();
    if (!members)
        throw std::runtime_error("MegaTextChat::getPeers() returned NULL");

    auto size = members->size();
    for (int i=0; i<size; i++)
        membs.emplace(members->getPeerHandle(i), members->getPeerPrivilege(i));
    return membs;
}

GroupChatRoom::Member::Member(GroupChatRoom& aRoom, const uint64_t& user, char aPriv)
: mRoom(aRoom), mPriv(aPriv)
{
    mNameAttrCbHandle = mRoom.mParent.client.userAttrCache.getAttr(user, mega::MegaApi::USER_ATTR_LASTNAME, this,
    [](Buffer* buf, void* userp)
    {
        auto self = static_cast<Member*>(userp);
        if (buf)
            self->mName.assign(buf->buf(), buf->dataSize());
        else if (self->mName.empty())
            self->mName = "\x07(error)";
        self->mRoom.updateTitle();
    });
}
GroupChatRoom::Member::~Member()
{
    mRoom.mParent.client.userAttrCache.removeCb(mNameAttrCbHandle);
}

}
