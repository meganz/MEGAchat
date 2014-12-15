#ifndef CSERVICES_H_INCLUDED
#define CSERVICES_H_INCLUDED

/* Plain C interface of the services library */

struct event_base;
struct event;

#ifdef __cplusplus
    #define MEGAIO_EXTERNC extern "C"
#else
    #define MEGAIO_EXTERNC
#endif

#ifdef MEGAIO_DLL
  #ifdef _WIN32
    #define MEGAIO_EXPORT MEGAIO_EXTERNC __declspec(dllexport)
    #define MEGAIO_IMPORT MEGAIO_EXTERNC __declspec(dllimport)
  #else
    #define MEGAIO_EXPORT MEGAIO_EXTERNC __attribute__ ((visibility ("default")))
    #define MEGAIO_IMPORT MEGAIO_EXPORT
  #endif

  #ifdef MEGAIO_BUILDING
     #define MEGAIO_IMPEXP MEGAIO_EXPORT
  #else
     #define MEGAIO_IMPEXP MEGAIO_IMPORT
  #endif
#else
//TODO: The below fix is temporary until services is moved in a dll
  #define MEGAIO_EXPORT MEGAIO_EXTERNC __attribute__ ((visibility ("default")))
  #define MEGAIO_IMPORT MEGAIO_EXTERNC
  #define MEGAIO_IMPEXP MEGAIO_EXTERNC
#endif
//Logging macros used by the services code
#define SVCS_LOG(fmtString,...) printf("Services: " fmtString "\n", ##__VA_ARGS__)
#define SVCS_LOG_ERROR(fmtString,...) SVCS_LOG("[E]" fmtString, ##__VA_ARGS__)
#define SVCS_LOG_WARNING(fmtString,...) SVCS_LOG("[W]" fmtString, ##__VA_ARGS__)
#define SVCS_LOG_INFO(fmtString,...) SVCS_LOG("[I]" fmtString, ##__VA_ARGS__)
#ifndef NDEBUG
    #define SVCS_LOG_DEBUG(fmtString,...) SVCS_LOG("[D]" fmtString, ##__VA_ARGS__)
#else
    #define SVC_LOG_DEBUG(fmtString,...)
#endif
//helper macros
#define _MEGA_STRLITERAL(x) _MEGA_STRLITERAL2(x)
#define _MEGA_STRLITERAL2(x) #x

/** Options bitmask for log flags */
enum {SVC_OPTIONS_LOGFLAGS = 0x000000ff};

/** Initialize and start the services engine
 @param postFunc The function that posts a void* to the application's message loop
 @param options Misc flags. The LS byte is reserved for logging bits
 (SVC_OPTIONS_LOGFLAGS mask). Currently supported flags:
     SVC_STROPHE_LOG: enabled logging of sent and received strophe stanzas.
*/
MEGAIO_IMPEXP int services_init(void(*postFunc)(void*), unsigned options);
MEGAIO_IMPEXP struct event_base* services_get_event_loop();
MEGAIO_IMPEXP int services_shutdown();

//Handle store

typedef unsigned int megaHandle; //invalid handle value is 0

enum
{
    MEGA_HTYPE_TIMER = 1,
    MEGA_HTYPE_DNSREQ = 2
};

/** Any common flags used for more than one service */
enum
{
/** Speficies that callbacks can be called by any thread,
* bypassing the Gui call marshalling mechanism. Normally for services-internal use
* only. \attention USE WITH CAUTION AND ONLY IF YOU KNOW WHAT YOU ARE DOING! */
    SVCF_NO_MARSHALL = 1,
/** Flags in bit positions higher than this can overlap across services.
 *  This should not be a problem if they are used only in the context of one service */
    SVCF_LAST = 1
};

MEGAIO_IMPEXP void* services_hstore_get_handle(unsigned short type, megaHandle handle);
MEGAIO_IMPEXP megaHandle services_hstore_add_handle(unsigned short type, void* ptr);
MEGAIO_IMPEXP int services_hstore_remove_handle(unsigned short type, megaHandle handle);

//select features to include
#ifndef SVC_DISABLE_STROPHE
    #include "cservices-strophe.h"
#endif

#ifndef SVC_DISABLE_DNS
    #include "cservices-dns.h"
#endif

#ifndef SVC_DISABLE_HTTP
    #include "cservices-http.h"
#endif

#endif
