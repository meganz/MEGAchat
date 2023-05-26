#include "waiter/libuvWaiter.h"

namespace mega {

static void break_libuv_loop(uv_async_t* handle)
{
    uv_stop(handle->loop);
}

LibuvWaiter::LibuvWaiter()
{
    evtloop = make_unique<uv_loop_t>();
    uv_loop_init(evtloop.get());
    
    asynchandle = make_unique<uv_async_t>();
    uv_async_init(evtloop.get(), asynchandle.get(), break_libuv_loop);
}

LibuvWaiter::~LibuvWaiter()
{
    // Request closing all active handles.
    uv_walk(evtloop.get(), [](uv_handle_t* handle, void*)
    {
        if (!uv_is_closing(handle))
        {
            uv_close(handle, [](uv_handle_t*){}); // clean-up no longer needed here
        }
    },
    nullptr);

    uv_run(evtloop.get(), UV_RUN_NOWAIT); // allow running uv_close() callbacks

    uv_loop_close(evtloop.get());
}

void LibuvWaiter::init(dstime ds)
{
    Waiter::init(ds);
}

int LibuvWaiter::wait()
{
    uv_run(evtloop.get(), UV_RUN_DEFAULT);
    return NEEDEXEC;
}

void LibuvWaiter::notify()
{
    uv_async_send(asynchandle.get());
}
    
} // namespace
