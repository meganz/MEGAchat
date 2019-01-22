#include "cservices.h"
#include "gcm.h"
#include <memory>
#include <thread>
#include <unordered_map>
#include <assert.h>
#include "cservices-thread.h"

#if defined(_WIN32) && defined(_MSC_VER)
#include <sys/timeb.h>  
#else
#include <sys/time.h>
#endif

namespace karere
{
    std::recursive_mutex timerMutex;
}

extern "C"
{
MEGAIO_EXPORT eventloop* services_eventloop = NULL;
MEGA_GCM_DLLEXPORT GcmPostFunc megaPostMessageToGui = NULL;

static void keepalive_timer_cb(uv_timer_t* handle) {}
    
MEGAIO_EXPORT eventloop* services_get_event_loop()
{
    return services_eventloop;
}

MEGAIO_EXPORT int services_init(GcmPostFunc postFunc, unsigned options)
{
    megaPostMessageToGui = postFunc;   
    services_eventloop = new uv_loop_t();
    uv_loop_init(services_eventloop);    
    uv_timer_t* timerhandle = new uv_timer_t();
    uv_timer_init(services_eventloop, timerhandle);
    uv_timer_start(timerhandle, keepalive_timer_cb, 1234567890ULL, 1);   
    return 0;
}

MEGAIO_EXPORT int services_shutdown()
{
    uv_stop(services_eventloop);       
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

int64_t services_get_time_ms()
{
#if defined(_WIN32) && defined(_MSC_VER)
    struct __timeb64 tb;
    _ftime64(&tb);
    return (tb.time * 1000) + (tb.millitm);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
#endif
}
}
