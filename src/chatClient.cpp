#include "chatRoom.h"
#include "contactList.h"
#include "ITypes.h" //for IPtr
#include "karereEventObjects.h"
#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mstrophepp.h>
#include "rtcModule/IRtcModule.h"
#include "rtcModule/lib.h"
#include "dummyCrypto.h"
#include "strophe.disco.h"
#include "base/services.h"
#include "sdkApi.h"
#include "megaCryptoFunctions.h"
#include "iEncHandler.h"
#include "messageBus.h"
#include "busConstants.h"
#include <common.h>
#include <upper_handler.h>
#include <shared_buffer.h>

#include <memory>
#include <map>
#include <type_traits>
#include "chatClient.h"
#include "textModule.h"

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
  api(new MyMegaApi("karere-native")),mEmail(email), mPassword(password), contactList(conn),
  mRtcHandler(NULL)
{}


Client::~Client()
{
    //when the strophe::Connection is destroyed, its handlers are automatically destroyed
}


void Client::registerRtcHandler(rtcModule::IEventHandler* rtcHandler)
{
    mRtcHandler.reset(rtcHandler);
}


promise::Promise<int> Client::init()
{
    /* get xmpp login from Mega API */
    return
    api->call(&mega::MegaApi::login, mEmail.c_str(), mPassword.c_str())
    .then([this](ReqResult result)
    {
        KR_LOG_DEBUG("Login to Mega API successful");
        return api->call(&mega::MegaApi::getUserData);
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Mega API login error: %s", err.what());
        return err;
    })
    .then([this](ReqResult result)
    {
        api->userData = result;
        const char* user = result->getText();
        if (!user || !user[0])
            return promise::reject<int>("Could not get our own JID");
        SdkString xmppPass = api->dumpXMPPSession();
        if (xmppPass.size() < 16)
            return promise::reject<int>("Mega session id is shorter than 16 bytes");
        ((char&)xmppPass.c_str()[16]) = 0;

        //xmpp_conn_set_keepalive(*conn, 10, 4);
        /* setup authentication information */
        std::string jid = std::string(user)+"@" KARERE_XMPP_DOMAIN "/kn_";
        jid.append(rtcModule::makeRandomString(10));
        xmpp_conn_set_jid(*conn, jid.c_str());
        xmpp_conn_set_pass(*conn, xmppPass.c_str());
        KR_LOG_DEBUG("xmpp user = '%s', pass = '%s'", jid.c_str(), xmppPass.c_str());
        setupHandlers();
// initiate connection
        return mega::retry(
        [this](int no)
        {
            return conn->connect(KARERE_DEFAULT_XMPP_SERVER, 0);
        },
        [this]()
        {
            xmpp_disconnect(*conn, -1);
        },
        KARERE_LOGIN_TIMEOUT, 0, KARERE_RECONNECT_DELAY_MAX, KARERE_RECONNECT_DELAY_INITIAL)
#if 0
//testing retrying of failed logins
        .fail([this](const promise::Error& err)
        {
            KR_LOG_WARNING("Done with retries, login");
            promise::Promise<int> pms;
            mega::setTimeout([this, pms]() mutable
            {
                conn->connect(KARERE_DEFAULT_XMPP_SERVER, 0)
                .then([pms](int ret) mutable
                { pms.resolve(ret); return 0;});
            }, 4000);
            return pms;
        });

#endif
    ;})
    .fail([](const promise::Error& error)
    {
        KR_LOG_ERROR("XMPP login error:\n%s", error.what());
        return error;
    })
    .then([this](int)
    {
        KR_LOG_INFO("XMPP login success");

// handle reconnect due to network errors
        setupReconnectHandler();

// Create and register the rtcmodule plugin
// the MegaCryptoFuncs object needs api->userData (to initialize the private key etc)
// To use DummyCrypto: new rtcModule::DummyCrypto(jid.c_str());
        rtc.reset(createRtcModule(*conn, mRtcHandler.get(), new rtcModule::MegaCryptoFuncs(*api), ""));
        conn->registerPlugin("rtcmodule", rtc.get());
// create and register text chat plugin
        mTextModule.reset(new TextModule(*this));
        conn->registerPlugin("textchat", mTextModule.get());
// create and register disco strophe plugin
        conn->registerPlugin("disco", new disco::DiscoPlugin(*conn, "Karere Native"));
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
        KR_LOG_DEBUG("contactlist initialized");
        //startKeepalivePings();
        return 0;
    })
    .fail([](const promise::Error& err)
    {
        KR_LOG_ERROR("Error initializing contactlist: %s", err.what());
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

    conn->addConnStateHandler(
       [this](xmpp_conn_t* conn_c, xmpp_conn_event_t event, int error,
        xmpp_stream_error_t* stream_error, void* userdata, bool& keepHandler) mutable
    {
        if ((event != XMPP_CONN_DISCONNECT) && (event != XMPP_CONN_FAIL))
            return;
        assert(xmpp_conn_get_state(conn_c) == XMPP_STATE_DISCONNECTED);
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
