#include <mstrophe.h>
#include <mstrophe-libevent.h>
#include "cservices.h"
#include "gcmpp.h"


static bool processMessage(void* arg, int what)
{
//    printf("received a message: %s\n", xmpp_events_to_str(what));
    if (what == -1) //strophe timer
        xmpp_on_timer_event((xmpp_evloop_api_t*)(arg), 1);
    else if (what >= 0)
        xmpp_on_conn_io_event((xmpp_conn_t*)(arg), what);
    return true;
}

static void eventcb(void* arg, int what)
{
//    printf("posted a message: %s\n", xmpp_events_to_str(what));
    mega::marshallCall([arg, what]() {processMessage(arg, what);});
}

static xmpp_ctx_t* gStropheContext = NULL;

MEGAIO_EXPORT int services_strophe_init(int options)
{
    xmpp_evloop_api_t* evloop = xmpp_libevent_evloop_new(
            services_get_event_loop(), eventcb);
    //  evloop->reset_waitflags = 0;
    /* init library */
    xmpp_initialize();

    /* create a context */
    xmpp_log_t* log = (options & SVC_STROPHE_LOG)
            ? xmpp_get_default_logger(XMPP_LEVEL_DEBUG)
            : nullptr; /* NULL silences output */
    gStropheContext = xmpp_ctx_new(NULL, evloop, log);
    return (gStropheContext != nullptr);
}

MEGAIO_IMPEXP xmpp_ctx_t* services_strophe_get_ctx()
{
    return gStropheContext;
}

MEGAIO_IMPEXP int services_strophe_shutdown()
{
    /* release our connection and context */
    xmpp_ctx_free(gStropheContext);
    /* final shutdown of the library */
    xmpp_shutdown();
    return 1;
}
