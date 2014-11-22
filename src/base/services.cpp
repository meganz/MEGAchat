#include "cservices.h"
#include "guiCallMarshaller.h"
#include <memory>
#include <thread>
#include <event2/event.h>
#include <event2/thread.h>

static struct event_base *eventloop = NULL;
std::unique_ptr<std::thread> libeventThread;

static void keepalive_timer_cb(evutil_socket_t fd, short what, void *arg){}

MEGAIO_EXPORT event_base* services_getLibeventLoop()
{
    return eventloop;
}

MEGAIO_EXPORT int services_init(void(*postFunc)(void *))
{
/*
    int ret = megaGcmInit_services(postFunc);
    if (ret != 0)
        return ret;
*/
#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    eventloop = event_base_new();
    evthread_make_base_notifiable(eventloop);
    struct event* keepalive = evtimer_new(eventloop, keepalive_timer_cb, NULL);
    struct timeval tv;
    tv.tv_sec = 123456;//0x7FFFFFFF;
    tv.tv_usec = 0;
    evtimer_add(keepalive, &tv);

    libeventThread.reset(new std::thread([]() mutable
    {
        /* enter the event loop */
        printf("libevent thread started, entering eventloop\n");
        event_base_loop(eventloop, 0);//EVLOOP_NO_EXIT_ON_EMPTY
        printf("libevent loop terminated\n");
    }));
}

MEGAIO_EXPORT int services_shutdown()
{
    event_base_loopexit(eventloop, NULL);
    printf("Terminating libevent thread...");
    libeventThread->join();
    libeventThread.reset();
    printf("done\n");
}
