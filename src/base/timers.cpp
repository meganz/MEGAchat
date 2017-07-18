#include "timers.hpp"
#include "megachatapi_impl.h"

#ifdef USE_LIBWEBSOCKETS
#include "waiter/libuvWaiter.h"
#else
#include "waiter/libeventWaiter.h"
#endif

namespace karere
{
    
#ifdef USE_LIBWEBSOCKETS

void init_uv_timer(void *ctx, uv_timer_t *timer)
{
    uv_timer_init(((mega::LibuvWaiter *)(((megachat::MegaChatApiImpl *)ctx)->waiter))->eventloop, timer);
}
    
#else

struct event_base *get_ev_loop(void *ctx)
{
    return ((mega::LibeventWaiter *)(((megachat::MegaChatApiImpl *)ctx)->waiter))->eventloop;
}

#endif

}

