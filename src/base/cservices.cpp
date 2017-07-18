#include "cservices.h"
#include "gcm.h"
#include <memory>
#include <thread>
#include <unordered_map>
#include <assert.h>
#include "cservices-thread.h"

extern "C"
{
MEGAIO_EXPORT eventloop* services_eventloop = NULL;
MEGA_GCM_DLLEXPORT GcmPostFunc megaPostMessageToGui = NULL;
t_svc_thread_handle libeventThread; //can't default-initialzie with pthreads - there is no reserved invalid value
t_svc_thread_id libeventThreadId;

bool hasLibeventThread = false;

#ifndef USE_LIBWEBSOCKETS
    static void keepalive_timer_cb(evutil_socket_t fd, short what, void *arg){}
#else
    static void keepalive_timer_cb(uv_timer_t* handle) {}
#endif
    
void globalInit(void(*postFunc)(void*, void*), uint32_t options, const char* logPath, size_t logSize)
{
    if (logPath)
    {
        karere::gLogger.logToFile(logPath, logSize);
    }
    services_init(postFunc, options);
}
    
void globalCleanup()
{
#ifndef KARERE_DISABLE_WEBRTC
    rtcModule::globalCleanup();
#endif
    services_shutdown();
}
    
MEGAIO_EXPORT eventloop* services_get_event_loop()
{
    return services_eventloop;
}

SVC_THREAD_FUNCDECL(libeventThreadFunc)
{
    /* enter the event loop */
    SVC_LOG_INFO("Libevent thread started, entering eventloop");
    
#ifndef USE_LIBWEBSOCKETS
    event_base_loop(services_eventloop, 0);//EVLOOP_NO_EXIT_ON_EMPTY
#else
    uv_run(services_eventloop, UV_RUN_DEFAULT);
#endif
    
    SVC_LOG_INFO("Libevent loop terminated");
    return (t_svc_thread_funcret)0;
}

MEGAIO_EXPORT int services_init(GcmPostFunc postFunc, unsigned options)
{
    megaPostMessageToGui = postFunc;
    
#ifndef USE_LIBWEBSOCKETS

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
#else
    services_eventloop = new uv_loop_t();
    uv_loop_init(services_eventloop);
    
    uv_timer_t* timerhandle = new uv_timer_t();
    uv_timer_init(services_eventloop, timerhandle);
    uv_timer_start(timerhandle, keepalive_timer_cb, 1234567890ULL, 1);
#endif
    
    hasLibeventThread = svc_thread_start(NULL, &libeventThread, &libeventThreadId, libeventThreadFunc);
    
    return 0;
}

MEGAIO_EXPORT int services_shutdown()
{
#ifndef USE_LIBWEBSOCKETS
    event_base_loopexit(services_eventloop, NULL);
#else    
    uv_stop(services_eventloop);
#endif
    
    SVC_LOG_INFO("Terminating libevent thread...");
    svc_thread_join(libeventThread);
    hasLibeventThread = false;
    SVC_LOG_INFO("Libevent thread terminated");
    return 0;
}

/*int64_t services_get_time_ms()
{
    struct timeval tv;
    evutil_gettimeofday(&tv, nullptr);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}*/
}
