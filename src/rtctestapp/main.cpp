#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mstrophe.h>
#include <mstrophe-libevent.h>
#include <mstrophepp.h>
#include <event2/event.h>

//#include "webrtc/base/ssladapter.h"

#include <QtGui/QApplication>
#include "mainwindow.h"
#include "../base/guiCallMarshaller.h"
#include "../IRtcModule.h"
#include "../lib.h"
#include "../DummyCrypto.h"
#include "../strophe.disco.h"
#include "../base/services.h"

using namespace std;
using namespace promise;

MainWindow* mainWin = NULL;
rtcModule::IRtcModule* rtc = NULL;
unique_ptr<rtcModule::ICryptoFunctions> crypto;
unique_ptr<rtcModule::IEventHandler> handler;
AppDelegate appDelegate;
bool processMessage(void* arg, int what);

MEGA_GCM_EXPORT void megaPostMessageToGui(void* msg)
{
    QMetaObject::invokeMethod(mainWin,
        "megaMessageSlot", Qt::QueuedConnection, Q_ARG(void*, msg));
}

using namespace strophe;

class RtcEventHandler: public rtcModule::IEventHandler
{
protected:
    strophe::Connection& mConn;
    disco::DiscoPlugin& mDisco;
public:

    RtcEventHandler(strophe::Connection& conn)
        :mConn(conn), mDisco(conn.plugin<disco::DiscoPlugin>("disco"))
    {}
    virtual void addDiscoFeature(const char* feature)
    {
        mDisco.addFeature(feature);
    }
};

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
        return 1;
    Stanza body(rawBody);
    if (!strcmp(s.attr("type"), "error")) return 1;

    auto intext = body.recursiveText();

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

void eventcb(void* arg, int what)
{
    printf("posted a message: %s\n", xmpp_events_to_str(what));
    mega::marshallCall([arg, what]() {processMessage(arg, what);});
}

bool processMessage(void* arg, int what)
{
    printf("received a message: %s\n", xmpp_events_to_str(what));
    if (what == -1) //timer
        xmpp_on_timer_event((xmpp_evloop_api_t*)(arg), 1);
    else if (what >= 0)
        xmpp_on_conn_io_event((xmpp_conn_t*)(arg), what);
    return true;
}
int ping(xmpp_conn_t * const pconn, void * const userdata)
{
   printf("TIMED HANDLER CALLED\n");
   strophe::Connection& conn = *static_cast<strophe::Connection*>(userdata);
   Stanza ping(conn);
   ping.setName("iq")
       .setAttr("type", "get")
       .setAttr("from", conn.jid())
       .c("ping", {{"xmlns", "urn:xmpp:ping"}});
   printf("sending ping\n");
   conn.sendIqQuery(ping, "set")
   .then([](Stanza pong)
   {
         printf("pong received\n");
         return 0;
   })
   .fail([](const promise::Error& err)
   {
       printf("Error receiving pong\n");
       return 0;
   });
   return 1;
}

xmpp_ctx_t *ctx = NULL;
shared_ptr<Connection> gConn;
int main(int argc, char **argv)
{
    xmpp_log_t *log;

    /* take a jid and password on the command line */
    if (argc != 3)
    {
        fprintf(stderr, "Usage: bot <jid> <pass>\n\n");
        return 1;
    }
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()));

    services_init(megaPostMessageToGui);
    xmpp_evloop_api_t* evloop = xmpp_libevent_evloop_new(
                services_getEventLoop(), eventcb);
  //  evloop->reset_waitflags = 0;
    /* init library */
    xmpp_initialize();

    /* create a context */
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG); /* pass NULL instead to silence output */
    ctx = xmpp_ctx_new(NULL, evloop, log);

    /* create a connection */
    gConn.reset(new strophe::Connection(ctx));
    Connection& conn = *(gConn.get());
    /* setup authentication information */
    xmpp_conn_set_jid(conn, argv[1]);
    xmpp_conn_set_pass(conn, argv[2]);
    conn.registerPlugin("disco", new disco::DiscoPlugin(conn, "Karere"));
    handler.reset(new RtcEventHandler(conn));

    /* create rtcModule */
    crypto.reset(new rtcModule::DummyCrypto(argv[1]));
//    rtc = createRtcModule(conn, handler.get(), crypto.get(), "");
//    conn.registerPlugin("rtcmodule", rtc);
    /* initiate connection */
    conn.connect(NULL, 0)
    .then([&](int)
    {
        printf("==========Connect promise resolved\n");
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


//    rtc::InitializeSSL();
    int ctr = 0;
    auto timer = mega::setTimeout([&ctr](){printf("onTimer\n"); ctr++;}, 2000);

    mega::setInterval([timer]()
    {
        auto ret = mega::cancelInterval(timer);
        printf("cancel: %d\n", ret);
    }, 1999);

    return a.exec();
}

void AppDelegate::onAppTerminate()
{
    printf("onAppTerminate\n");
    services_shutdown();
    gConn.reset();
    /* release our connection and context */
    xmpp_ctx_free(ctx);
    /* final shutdown of the library */
    xmpp_shutdown();
}
