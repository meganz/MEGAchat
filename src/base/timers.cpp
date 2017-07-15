#include "timers.hpp"
#include "megachatapi_impl.h"
#include "waiter/libuvWaiter.h"

#if defined(USE_LIBWEBSOCKETS) && defined(LWS_USE_LIBUV)

namespace karere
{
void init_uv_timer(void *ctx, uv_timer_t *timer)
{
    uv_timer_init(((mega::LibuvWaiter *)(((megachat::MegaChatApiImpl *)ctx)->waiter))->eventloop, timer);
}
}

#endif
