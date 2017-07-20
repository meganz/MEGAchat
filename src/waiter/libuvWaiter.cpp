#include "waiter/libuvWaiter.h"

namespace mega {

static void break_libuv_loop(uv_async_t* handle)
{
    uv_stop(handle->loop);
}

LibuvWaiter::LibuvWaiter()
{
    eventloop = new uv_loop_t();
    uv_loop_init(eventloop);
    
    asynchandle = new uv_async_t();
    uv_async_init(eventloop, asynchandle, break_libuv_loop);
}

LibuvWaiter::~LibuvWaiter()
{
    uv_close((uv_handle_t*)asynchandle, NULL);
    uv_run(eventloop, UV_RUN_DEFAULT);
    uv_loop_close(eventloop);
    delete asynchandle;
    delete eventloop;
}

void LibuvWaiter::init(dstime ds)
{
    Waiter::init(ds);
}

int LibuvWaiter::wait()
{
    uv_run(eventloop, UV_RUN_DEFAULT);
    return NEEDEXEC;
}

void LibuvWaiter::notify()
{
    uv_async_send(asynchandle);
}
    
} // namespace
