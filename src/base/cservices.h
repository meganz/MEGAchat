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


MEGAIO_IMPEXP int services_init(void(*postFunc)(void*));
MEGAIO_IMPEXP struct event_base* services_getEventLoop();
MEGAIO_IMPEXP int services_shutdown();

//Handle store

typedef unsigned int megaHandle; //invalid handle value is 0

enum
{
    MEGA_HTYPE_TIMER = 1
};

MEGAIO_IMPEXP void* services_hstore_get_handle(unsigned short type, megaHandle handle);
MEGAIO_IMPEXP megaHandle services_hstore_add_handle(unsigned short type, void* ptr);
MEGAIO_IMPEXP int services_hstore_remove_handle(unsigned short type, megaHandle handle);

#endif
