#ifndef GUICALLMARSHALLER_H
#define GUICALLMARSHALLER_H

/* This is a plain C header */

#ifdef _WIN32
    #ifndef MEGA_FULL_STATIC
        #define MEGA_GCM_DLLEXPORT __declspec(dllexport)
        #define MEGA_GCM_DLLIMPORT __declspec(dllimport)
    #else
        #define MEGA_GCM_DLLEXPORT 
        #define MEGA_GCM_DLLIMPORT 
    #endif
#else
    #define MEGA_GCM_DLLEXPORT __attribute__ ((visibility("default")))
    #define MEGA_GCM_DLLIMPORT MEGA_GCM_DLLEXPORT
#endif

#ifdef MEGA_SERVICES_BUILDING
    #define MEGA_GCM_IMPEXP MEGA_GCM_DLLEXPORT
#else
    #define MEGA_GCM_IMPEXP MEGA_GCM_DLLIMPORT
#endif

typedef void(*megaMessageFunc)(void*);

/** The Gui Call Marshaller mechanism supports marshalling of a plain C function
 * call with signature \c void(void*) together with an arbitrary data stuct.
 * The function pointer is the first member of the struct, and a C struct having
 * only that member is typedef-ed as megaMessage. Adding data members to the structure is
 * done by deriving in C++ from the megaMessage struct. The \c void(void*) function
 * is called the \c handler.
 * As both the handler and message structure are normally provided at the same
 * place in user code (or even done automatically by e.g. a templated function),
 * the handler knows the actual type of the structure, so it can cast it to the
 * proper \c megaMessage-derived type and access the additional data after the
 * \c megaMessage header. In other words, this is polymorphism in plain C,
 * and this API is plain C to make possible interoperation
 * between modules built with different compilers, even different languages.
 *
 * All memory associated with the message object is managed on the same side of the
 * DLL boundary - the one that posts the message. It is that side that allocated
 * the memory for the message object, and only it knows its exact type.
*/

struct megaMessage
{
    megaMessageFunc func;
    /** If we don't provide an initializing constructor, operator new() will initialize
     * func to NULL, and then we will overwrite it, which is inefficient. That's why we
     * implement a constructor in case we are included in C++ code
     */
     #ifdef __cplusplus
         megaMessage(megaMessageFunc aFunc): func(aFunc){}
     #endif
};
//enum {kMegaMsgMagic = 0x3e9a3591};

#ifdef __cplusplus
#define GCM_EXTERNC extern "C"
extern "C" {
#else
#define GCM_EXTERNC
#endif

/** This is the type of the function that posts a megaMessage to the GUI thread */
typedef void (*GcmPostFunc)(void*, void*);

/** This function posts an opaque \c void* to the application's (GUI) message loop.
* That message is then received by the application's main (GUI) thread and
* the user's code is responsible to send it to \c megaProcessMessage(void*)
* to be processed by the GCM mechanism on the GUI thread.
* \warning This function must be provided by the user and is specific to the GUI framework
* used in the app. It is called by various threads when they need to execute a function
* call on the main (GUI) thread.
*/
extern MEGA_GCM_IMPEXP GcmPostFunc megaPostMessageToGui;

/** When the application's main (GUI) thread receives a message posted by
 * megaPostMessageToGui(), the user's code must forward the \c void* pointer
 * from that message to this function for further processing. This function is
 * called by a handler in the app's (GUI) event/message loop (or equivalent).
* \warning Must be called only from the GUI thread
*/
static inline void megaProcessMessage(void* vptr)
{
    struct megaMessage* msg = (struct megaMessage*)vptr;
    msg->func(vptr);
}

#ifdef __cplusplus
} //end extern "C"
#endif

#endif // GUICALLMARSHALLER_H
