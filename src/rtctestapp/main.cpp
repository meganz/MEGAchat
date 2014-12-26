#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mstrophepp.h>
#include <event2/event.h>

//#include "webrtc/base/ssladapter.h"

#include <QtGui/QApplication>
#include "mainwindow.h"
#include "../base/gcm.h"
#include "../IRtcModule.h"
#include "../lib.h"
#include "../DummyCrypto.h"
#include "../strophe.disco.h"
#include "../base/services.h"
#include "sdkApi.h"
#include "base32.h"

using namespace std;
using namespace promise;

MainWindow* mainWin = NULL;
rtcModule::IRtcModule* rtc = NULL;
unique_ptr<rtcModule::ICryptoFunctions> crypto;
unique_ptr<rtcModule::IEventHandler> handler;
unique_ptr<MyMegaApi> api(new MyMegaApi("sdfsdfsdf"));

struct GcmEvent: public QEvent
{
    static const QEvent::Type type;
    void* ptr;
    GcmEvent(void* aPtr): QEvent(type), ptr(aPtr){}
};
const QEvent::Type GcmEvent::type = (QEvent::Type)QEvent::registerEventType();

class AppDelegate: public QObject
{
    Q_OBJECT
public slots:
    void onAppTerminate();
public:
    virtual bool event(QEvent* event)
    {
        if (event->type() != GcmEvent::type)
            return false;

        megaProcessMessage(static_cast<GcmEvent*>(event)->ptr);
        return true;
    }
};

AppDelegate appDelegate;

MEGA_GCM_EXPORT void megaPostMessageToGui(void* msg)
{
    QEvent* event = new GcmEvent(msg);
    QApplication::postEvent(&appDelegate, event);
}

using namespace strophe;


void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
    mega::marshallCall([]{mainWin->close();});
}

auto message_handler = [](Stanza s, void* userdata, bool& keep)
{
    strophe::Connection& conn = *static_cast<strophe::Connection*>(userdata);
    xmpp_stanza_t* rawBody = s.rawChild("body");
    if (!rawBody)
        return;
    Stanza body(rawBody);
    if (!strcmp(s.attr("type"), "error")) return;

    auto intext = body.text();

    printf("Incoming message from %s: %s\n", s.attr("from"), intext.c_str());

    Stanza reply(conn);
    reply.init("message", {
        {"type", s.attrDefault("type", "chat")},
        {"to", s.attr("from")}
    })
    .c("body", {})
    .t(intext.c_str()+std::string(" to you too!"));

    conn.send(reply);
};

int ping(xmpp_conn_t * const pconn, void * const userdata)
{
   strophe::Connection& conn = *static_cast<strophe::Connection*>(userdata);
   Stanza ping(conn);
   ping.setName("iq")
       .setAttr("type", "get")
       .setAttr("from", conn.jid())
       .c("ping", {{"xmlns", "urn:xmpp:ping"}});
   conn.sendIqQuery(ping, "set")
   .then([](Stanza pong)
   {
         return 0;
   })
   .fail([](const promise::Error& err)
   {
       printf("Error receiving pong\n");
       return 0;
   });
   return 1;
}

string jid;
const char* pass = NULL;
string peer;
bool inCall = false;

int main(int argc, char **argv)
{
    /* take a jid and password on the command line */
    if (argc != 3)
    {
        fprintf(stderr, "Usage: rtctestapp <user> <peer>\n\n");
        return 1;
    }
    const string serverpart = "@j100.server.lu";
    jid = argv[1]+serverpart;
    pass = "testpass";
    peer = argv[2]+serverpart;
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

    services_init(megaPostMessageToGui, SVC_STROPHE_LOG);

    /* create a connection */
    mainWin->mConn.reset(new strophe::Connection(services_strophe_get_ctx()));

//get xmpp login from Mega API
    api->call(&MegaApi::login, "lpetrov+mega14@me.com", "megarullz")
    .then([](ReqResult result)
    {
        printf("login success\n");
        return api->call(&MegaApi::getUserData);
    })
    .then([](ReqResult result)
    {
        auto user = result->getText();

        AutoString pass = api->dumpXMPPSession();
        auto len = strlen(pass.c_str());
        if (len < 16)
            return promise::reject<int>("Session id is shorter than 16 bytes");
        ((char&)pass.c_str()[16]) = 0;

        Connection& conn = *(mainWin->mConn.get());
        /* setup authentication information */
        string xmppuser = (string(user)+"@developers.mega.co.nz/karerenative");
        xmpp_conn_set_jid(conn, xmppuser.c_str());
        xmpp_conn_set_pass(conn, pass.c_str());
        printf("user = '%s', pass = '%s'\n", xmppuser.c_str(), pass.c_str());

        conn.registerPlugin("disco", new disco::DiscoPlugin(conn, "Karere"));
        handler.reset(new RtcEventHandler(mainWin));

    /* create rtcModule */
        crypto.reset(new rtcModule::DummyCrypto(jid.c_str()));
        rtc = createRtcModule(conn, handler.get(), crypto.get(), "");
        rtc->updateIceServers("url=turn:j100.server.lu:3591?transport=udp, user=alex, pass=alexsecret");
        conn.registerPlugin("rtcmodule", rtc);
        /* initiate connection */
        return conn.connect("karere-001.developers.mega.co.nz", 0);
    })
    .then([&](int)
    {
        printf("==========Connect promise resolved\n");
        Connection& conn = *(mainWin->mConn.get());
        xmpp_timed_handler_add(conn, ping, 100000, &conn);
        conn.addHandler(message_handler, NULL, "message", NULL, NULL, &conn);
    /* Send initial <presence/> so that we appear online to contacts */
        Stanza pres(conn);
        pres.setName("presence");
        conn.send(pres);
        return 0;
    })
    .fail([](const promise::Error& error)
    {
        printf("==========Connect promise failed\n");
        return error;
    });
    signal(SIGINT, sigintHandler);
    return a.exec();
}

void AppDelegate::onAppTerminate()
{
    printf("onAppTerminate\n");
    rtc->destroy();
    rtcCleanup();
    services_shutdown();
    mainWin->mConn.reset();
}
#include <main.moc>
