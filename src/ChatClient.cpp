#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mstrophepp.h>
#include "../IRtcModule.h"
#include "../lib.h"
#include "../DummyCrypto.h"
#include "../strophe.disco.h"
#include "../base/services.h"
#include "sdkApi.h"
#include "MegaCryptoFunctions.h"
#include "ChatClient.h"

using namespace std;
using namespace promise;
using namespace mega;
using namespace strophe;

namespace karere
{
Client::Client(const string& email, const string& password,
               rtcModule::IEventHandler* rtcHandler)
    :mEmail(email), mPassword(password),
    conn(new strophe::Connection(services_strophe_get_ctx())),
    contactList(*this),
    mApi(new MyMegaApi("sdfsdfsdf")), mRtcHandler(rtcHandler)
{
}
Promise<int> Client::start()
{
/* get xmpp login from Mega API */
    return
    mApi->call(&MegaApi::login, mEmail.c_str(), mPassword.c_str())
    .then([this](ReqResult result)
    {
        KR_LOG_DEBUG("Login to Mega API successful");
        return mApi->call(&MegaApi::getUserData);
    })
    .then([this](ReqResult result)
    {
        mApi->userData = result;
        const char* user = result->getText();
        if (!user || !user[0])
            return promise::reject<int>("Could not get our own JID");
        SdkString xmppPass = mApi->dumpXMPPSession();
        if (xmppPass.size() < 16)
            return promise::reject<int>("Mega session id is shorter than 16 bytes");
        ((char&)xmppPass.c_str()[16]) = 0;

/* setup authentication information */
        string jid = string(user)+"@" KARERE_XMPP_DOMAIN;
        xmpp_conn_set_jid(*conn, jid.c_str());
        xmpp_conn_set_pass(*conn, xmppPass.c_str());
        KR_LOG("user = '%s', pass = '%s'", jid.c_str(), xmppPass.c_str());
/* initiate connection */
        return conn->connect(KARERE_DEFAULT_XMPP_SERVER, 0);
    })
    .then([this](int)
    {
        printf("==========Connect promise resolved\n");
//Create the RtcModule object
        //the MegaCryptoFuncs object needs mApi->userData (to initialize the private key etc)
        mRtc.reset(createRtcModule(
          *conn, mRtcHandler.get(), new rtcModule::MegaCryptoFuncs(*mApi), //new rtcModule::DummyCrypto(jid.c_str());
          ""));
        conn->registerPlugin("disco", new disco::DiscoPlugin(*conn, "Karere Native"));
        conn->registerPlugin("rtcmodule", mRtc.get());
        if (onRtcInitialized)
            onRtcInitialized();
//===
        auto pms = contactList.init();
/* Send initial <presence/> so that we appear online to contacts */
        Stanza pres(*conn);
        pres.setName("presence");
        conn->send(pres);
        return pms;
    })
    .fail([](const promise::Error& error)
    {
        printf("==========Connect promise failed:\n%s\n", error.msg().c_str());
        return error;
    });
}

void Client::startSelfPings(int intervalSec)
{
    mega::setInterval([this]()
    {
        Stanza ping(*conn);
        ping.setName("iq")
            .setAttr("type", "get")
            .setAttr("from", conn->jid())
                .c("ping", {{"xmlns", "urn:xmpp:ping"}});
        conn->sendIqQuery(ping, "set")
        .then([](Stanza s)
        {
            return 0;
        })
        .fail([](const promise::Error& err)
        {
            printf("Error receiving pong\n");
            return 0;
        });
    }, intervalSec*1000);
}
}
