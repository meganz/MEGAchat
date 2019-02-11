#ifndef CSERVICES_H_INCLUDED
#define CSERVICES_H_INCLUDED
#include <uv.h>

typedef uv_loop_t eventloop;
typedef uv_timer_t timerevent;

/* Plain C interface of the services library */
#include <logger.h>

struct event_base;
struct event;
#ifdef __cplusplus
extern "C" {
#endif

#ifdef MEGAIO_DLL
  #ifdef _WIN32
    #define MEGAIO_EXPORT __declspec(dllexport)
    #define MEGAIO_IMPORT __declspec(dllimport)
  #else //non-win32
    #define MEGAIO_EXPORT __attribute__ ((visibility ("default")))
    #define MEGAIO_IMPORT MEGAIO_EXPORT
  #endif
#else //not a DLL
    #define MEGAIO_EXPORT
    #define MEGAIO_IMPORT
#endif

#ifdef services_EXPORTS
    #define MEGAIO_IMPEXP MEGAIO_EXPORT
#else
    #define MEGAIO_IMPEXP MEGAIO_IMPORT
#endif

//Logging macros used by the services code
#define SVC_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_services, fmtString, ##__VA_ARGS__)
#define SVC_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_services, fmtString, ##__VA_ARGS__)
#define SVC_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_services, fmtString, ##__VA_ARGS__)
#define SVC_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_services, fmtString, ##__VA_ARGS__)
//helper macros
#define _MEGA_STRLITERAL(x) _MEGA_STRLITERAL2(x)
#define _MEGA_STRLITERAL2(x) #x

/** Options bitmask for log flags */
enum {SVC_OPTIONS_LOGFLAGS = 0x000000ff};

/** The global, singleton eventloop object. */
extern MEGAIO_IMPEXP eventloop* services_eventloop;

/**
 * @brief Initialize and start the services engine
 * @param postFunc The function that posts a void* to the application's message loop
 * @param options Misc flags. The LS byte is reserved for logging bits
 * (SVC_OPTIONS_LOGFLAGS mask).
 */
MEGAIO_IMPEXP int services_init(void(*postFunc)(void*, void*), unsigned options);

MEGAIO_IMPEXP eventloop* services_get_event_loop();

/** @brief Shuts down the services engine. Call this before terminating the application */
MEGAIO_IMPEXP int services_shutdown();

//Handle store

typedef unsigned int megaHandle; //invalid handle value is 0

enum
{
    MEGA_HTYPE_TIMER = 1,
    MEGA_HTYPE_DNSREQ = 2
};

/** @brief Service configuration flags that are common for all services */
enum
{
    /** @brief Speficies that callbacks can be called by any thread,
     * bypassing the Gui call marshalling mechanism. Normally for services-internal use
     * only. \attention USE WITH CAUTION AND ONLY IF YOU KNOW WHAT YOU ARE DOING!
     */
    SVCF_NO_MARSHALL = 1,

    /** @brief Flags in bit positions higher than this can overlap across services.
    *  This should not be a problem if they are used only in the context of one service
    */
    SVCF_LAST = 1
};

MEGAIO_IMPEXP void* services_hstore_get_handle(unsigned short type, megaHandle handle);
MEGAIO_IMPEXP megaHandle services_hstore_add_handle(unsigned short type, void* ptr);
MEGAIO_IMPEXP int services_hstore_remove_handle(unsigned short type, megaHandle handle);
//==
MEGAIO_IMPEXP int64_t services_get_time_ms();

#ifdef __cplusplus
}
#endif

#endif
