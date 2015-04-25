#include "cservices.h"
#include "gcm.h"
#include <memory>
#include <thread>
#include <unordered_map>
#include <event2/event.h>
#include <event2/thread.h>
#include <assert.h>
#include "cservices-thread.h"
#include "cservices.h"

struct event_base* services_eventloop = NULL;
MEGA_GCM_DLLEXPORT GcmPostFunc megaPostMessageToGui = NULL;
t_svc_thread_handle libeventThread; //can't initialzie with pthreads - there is no reserved invalid value
t_svc_thread_id libeventThreadId;
bool hasLibeventThread = false;

static void keepalive_timer_cb(evutil_socket_t fd, short what, void *arg){}

MEGAIO_EXPORT event_base* services_get_event_loop()
{
    return services_eventloop;
}

SVC_THREAD_FUNCDECL(libeventThreadFunc)
{
    /* enter the event loop */
    SVC_LOG_INFO("libevent thread started, entering eventloop");
    event_base_loop(services_eventloop, 0);//EVLOOP_NO_EXIT_ON_EMPTY
    SVC_LOG_INFO("libevent loop terminated");
    return (t_svc_thread_funcret)0;

}

MEGAIO_EXPORT int services_init(GcmPostFunc postFunc, unsigned options)
{
    megaPostMessageToGui = postFunc;
#ifdef _WIN32
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    services_eventloop = event_base_new();
    evthread_make_base_notifiable(services_eventloop);
    struct event* keepalive = evtimer_new(services_eventloop, keepalive_timer_cb, NULL);
    struct timeval tv;
    tv.tv_sec = 123456;//0x7FFFFFFF;
    tv.tv_usec = 0;
    evtimer_add(keepalive, &tv);
#ifndef SVC_DISABLE_DNS
    services_dns_init(options);
#endif
#ifndef SVC_DISABLE_STROPHE
    services_strophe_init(options);
#endif
#ifndef SVC_DISABLE_HTTP
    services_http_init(options);
#endif
    hasLibeventThread = svc_thread_start(
                NULL, &libeventThread, &libeventThreadId, libeventThreadFunc);
    return 0;
}

MEGAIO_EXPORT int services_shutdown()
{
#ifndef SVC_DISABLE_HTTP
    services_http_shutdown();
#endif
    event_base_loopexit(services_eventloop, NULL);
    SVC_LOG_INFO("Terminating libevent thread...");
    svc_thread_join(libeventThread);
    hasLibeventThread = false;
    SVC_LOG_INFO("libevent thread terminated");
    return 0;
}

//Handle store

struct HandleItem
{
    unsigned short type;
    void* ptr;
    HandleItem(unsigned short aType, void* aPtr):type(aType), ptr(aPtr){}
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

MEGAIO_EXPORT megaHandle services_hstore_add_handle(unsigned short type, void* ptr)
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
    bool inserted =
#endif
    gHandleStore.emplace(std::piecewise_construct,
        std::forward_as_tuple(id), std::forward_as_tuple(type, ptr))
#ifndef NDEBUG
        .second
#endif
               ;
    assert(inserted);
    return id;
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
        fprintf(stderr, "ERROR: services_hstore_remove_handle: Handle found, but requested type %u does not match actual type %u\n", type, it->second.type);
        fflush(stderr);
        return 0;
    }
    gHandleStore.erase(it);
    return 1;
}
