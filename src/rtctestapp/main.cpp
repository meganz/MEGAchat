#ifdef _WIN32
    #include <winsock2.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <mstrophe.h>
#include <mstrophe-libevent.h>

#include <event2/event.h>
#include <event2/thread.h>
#include <mstrophepp.h>

//#include "webrtc/base/ssladapter.h"

#include <QtGui/QApplication>
#include "mainwindow.h"
#include "../base/guiCallMarshaller.h"

using namespace std;
using namespace std;
using namespace promise;

MainWindow* mainWin = NULL;


AppDelegate appDelegate;
bool processMessage(void* arg, int what);
void terminateApp();

namespace mega
{
    void postMessageToGui(void* msg)
    {
        QMetaObject::invokeMethod(mainWin,
            "megaMessageSlot", Qt::QueuedConnection, Q_ARG(void*, msg));
    }
}

using namespace strophe;
struct event_base *base = NULL;
bool term = 0;
shared_ptr<thread> watcherThread;
void sigintHandler(int)
{
    printf("SIGINT Received\n");
    mega::marshalCall([]() {processMessage(nullptr, -2);});
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
    mega::marshalCall([arg, what]() {processMessage(arg, what);});
}

bool processMessage(void* arg, int what)
{
    printf("received a message: %s\n", xmpp_events_to_str(what));
    if (what == -2) //terminate
    {
        terminateApp();
        return false;
    }
    else if (what == -1) //timer
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

void keepalive_timer_cb(evutil_socket_t fd, short what, void *arg)
{}

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

#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    base = event_base_new();
    evthread_make_base_notifiable(base);
    struct event* keepalive = evtimer_new(base, keepalive_timer_cb, NULL);
    struct timeval tv;
    tv.tv_sec = 123456;//0x7FFFFFFF;
    tv.tv_usec = 0;
    evtimer_add(keepalive, &tv);
    xmpp_evloop_api_t* evloop = xmpp_libevent_evloop_new(base, eventcb);
  //  evloop->reset_waitflags = 0;
    /* init library */
    xmpp_initialize();

    /* create a context */
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG); /* pass NULL instead to silence output */
    ctx = xmpp_ctx_new(NULL, evloop, log);

    /* create a connection */
    gConn.reset(new strophe::Connection(ctx));
    Connection& conn = *gConn;
    /* setup authentication information */
    xmpp_conn_set_jid(conn, argv[1]);
    xmpp_conn_set_pass(conn, argv[2]);
    /* initiate connection */
    conn.connect(NULL, 0)
    .then([&](int)
    {
        printf("==========Connect promise resolved\n");
        xmpp_timed_handler_add(conn, ping, 1000, &conn);
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
  //  signal(SIGINT, sigintHandler);


//    rtc::InitializeSSL();
    QApplication a(argc, argv);
    mainWin = new MainWindow;
    mainWin->show();
    QObject::connect( qApp, SIGNAL(lastWindowClosed()), &appDelegate, SLOT(onAppTerminate()) );
    watcherThread.reset(new std::thread([]() mutable
    {
        /* enter the event loop -
        our connect handler will trigger an exit */
        printf("Watcher thread started\n");
        event_base_loop(base, 0);//EVLOOP_NO_EXIT_ON_EMPTY
        printf("eventloop exited\n");
        term = 1;
    }));

    return a.exec();
}

void AppDelegate::onAppTerminate()
{
    printf("onAppTerminate\n");
    mega::marshalCall([]() {processMessage(nullptr, -2);});
}

void terminateApp()
{
    event_base_loopexit(base, NULL);
    printf("joining thread\n");
    watcherThread->join();
    gConn.reset();
    /* release our connection and context */
    xmpp_ctx_free(ctx);
    /* final shutdown of the library */
    xmpp_shutdown();
}
