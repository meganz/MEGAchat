#include <mstrophe.h>
#include <mstrophe-libevent.h>
#include "cservices.h"
#include "gcmpp.h"
#include "logger.h"

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
    karere::marshallCall([arg, what]() {processMessage(arg, what);});
}

static xmpp_ctx_t* gStropheContext = NULL;
unsigned stropheToKarereLogLevels[4];

MEGAIO_EXPORT int services_strophe_init(int options)
{
    stropheToKarereLogLevels[XMPP_LEVEL_DEBUG] = krLogLevelDebug;
    stropheToKarereLogLevels[XMPP_LEVEL_INFO] = krLogLevelInfo;
    stropheToKarereLogLevels[XMPP_LEVEL_WARN] = krLogLevelWarn;
    stropheToKarereLogLevels[XMPP_LEVEL_ERROR] = krLogLevelError;

    xmpp_evloop_api_t* evloop = xmpp_libevent_evloop_new(
            services_get_event_loop(), eventcb);
    //  evloop->reset_waitflags = 0;
    /* init library */
    xmpp_initialize();
    /* create a context */
    static xmpp_log_t log =
    {
        [](void* userdata, const xmpp_log_level_t level, const char* area, const char* msg)
        {
            if (area && (area[0] == 'x') && (area[1] =='m') && (area[2] == 'p') && (area[3] == 'p'))
                KARERE_LOG(krLogChannel_xmpp, stropheToKarereLogLevels[level], "%s", msg);
            else
                KARERE_LOG(krLogChannel_strophe, stropheToKarereLogLevels[level], "[%s]: %s", area, msg);
        },
        nullptr
    };
    gStropheContext = xmpp_ctx_new(NULL, evloop, &log);
    return (gStropheContext != nullptr);
}

MEGAIO_EXPORT xmpp_ctx_t* services_strophe_get_ctx()
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
