#include "waiter/libeventWaiter.h"

namespace mega {
    
static void keepalive_cb(evutil_socket_t fd, short what, void *arg) { }
    
LibeventWaiter::LibeventWaiter()
{
#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    
    eventloop = event_base_new();
    evthread_make_base_notifiable(eventloop);
    
    keepalive = evtimer_new(eventloop, keepalive_cb, NULL);
    struct timeval tv;
    tv.tv_sec = 123456;//0x7FFFFFFF;
    tv.tv_usec = 0;
    evtimer_add(keepalive, &tv);
}

LibeventWaiter::~LibeventWaiter()
{
    if (keepalive)
    {
        evtimer_del(keepalive);
        event_free(keepalive);
    }
    if (eventloop)
    {
        event_base_free(eventloop);
    }
}

void LibeventWaiter::init(dstime ds)
{
    Waiter::init(ds);
}

int LibeventWaiter::wait()
{
    event_base_loop(eventloop, 0);
    return NEEDEXEC;
}

void LibeventWaiter::notify()
{
    event_base_loopexit(eventloop, NULL);
}
} // namespace
