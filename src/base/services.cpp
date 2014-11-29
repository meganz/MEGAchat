#include "cservices.h"
#include "guiCallMarshaller.h"
#include <memory>
#include <thread>
#include <unordered_map>
#include <event2/event.h>
#include <event2/thread.h>

static struct event_base *eventloop = NULL;
std::unique_ptr<std::thread> libeventThread;

static void keepalive_timer_cb(evutil_socket_t fd, short what, void *arg){}

MEGAIO_EXPORT event_base* services_get_event_loop()
{
    return eventloop;
}

MEGAIO_EXPORT int services_init(void(*postFunc)(void *), unsigned options)
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
#ifndef SVC_DISABLE_STROPHE
    services_strophe_init(options & SVC_OPTIONS_LOGFLAGS);
#endif
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

//Handle store

struct HandleItem
{
    void* ptr;
    unsigned short type;
    HandleItem(unsigned short aType, void* aPtr):ptr(aPtr), type(aType){}
    bool operator==(const HandleItem& other) const
    {
        return ((type == other.type) && (ptr == other.ptr));
    }
};

std::unordered_map<megaHandle, HandleItem> gHandleStore;
megaHandle gHandleCtr = 0;

MEGAIO_EXPORT void* services_hstore_get_handle(unsigned short type, megaHandle handle)
{
    auto it = gHandleStore.find(handle);
    if ((it == gHandleStore.end()) || (it->second.type != type))
        return nullptr;
    return it->second.ptr;
}

MEGAIO_EXPORT megaHandle services_hstore_add_handle(unsigned short type, void* handle)
{
#ifndef NDEBUG
    megaHandle old = gHandleCtr;
#endif
    megaHandle id = ++gHandleCtr;
#ifndef NDEBUG
    if (id < old)
    {
        fprintf(stderr, "ERROR: megaHandle id generator wrapped around\n");
        fflush(stderr);
        abort();
    }
#endif
    return gHandleStore.emplace(std::piecewise_construct,
        std::forward_as_tuple(id), std::forward_as_tuple(type, handle)).second;
}

MEGAIO_EXPORT int services_hstore_remove_handle(unsigned short type, megaHandle handle)
{
    auto it = gHandleStore.find(handle);
    if (it == gHandleStore.end())
    {
#ifndef NDEBUG
        fprintf(stderr, "ERROR: services_hstore_remove_handle: Handle not found (id=%u, type=%d)\n", handle, type);
#endif
        return 0;
    }
    if (it->second.type != type)
    {
        fprintf(stderr, "ERROR: services_hstore_remove_handle: Handle found, but requested type %d does not match actual type %d\n", it->second.type, type);
        fflush(stderr);
        return 0;
    }
    gHandleStore.erase(it);
    return 1;
}
