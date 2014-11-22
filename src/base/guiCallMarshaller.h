#ifndef GUICALLMARSHALLER_H
#define GUICALLMARSHALLER_H

/* This is a plain C header */

#ifdef _WIN32
#define MEGA_GCM_EXPORT __declspec(dllexport)
#else
#define MEGA_GCM_EXPORT __attribute__ ((visibility ("default")))
#endif

struct megaMessage;
typedef void(*megaMessageFunc)(struct megaMessage*);

#ifdef __cplusplus
extern "C"
{
#endif

/** The mechanism supports marshalling of a plain C function with signature void(void*)
 * The Message structure carries the function pointer and the data is a pointer to
 * the Message struct itself. The actual object passed is derived from \c Message and can
 * contain arbitrary data, so \c struct \c Message acts as the header of the actual structure
 * passed. As the handler knows the actual type of the structure, it casts it to the proper
 * derived type and accesses the additional data after the \c Message header. In other words,
 * this is plain C polymorphism, and this API is plain C to make possible interoperation
 * between modules built with different compilers, even different languages.
 *
 * All memory associated with the message object is managed on the same side of the
 * shared-object boundary (the one that posts the message),
 * because only that side knows the exact type of the message
 * object, and also because each shared object is likely to
 * have its own memory management and even be built with a different compiler (C++ ABIs
 * are incompatible).
*/

struct megaMessage
{
    megaMessageFunc func;
#ifdef __cplusplus
//if we don't provice an initializing constructor, operator new() will initialize func
//to NULL, and then we will overwrite it, which is inefficient.
    megaMessage(megaMessageFunc aFunc): func(aFunc){}
#endif
};
//enum {kMegaMsgMagic = 0x3e9a3591};

/** This function posts an opaque \c void* to the application's (GUI) message loop
* and that message is then received by the application's main (GUI) thread and sent to
* the \c megaProcessMessage(void*) to be handled by the receiving code.
* \warning This function must be provided by the user and is specific to the GUI framework
* used in the app. It is called by various threads when they need to execute a function
* call by the main (GUI) thread.
*/

#ifndef MEGA_GCM_CLIENT_INIT_API
    void megaPostMessageToGui(void* msg);
#else
    typedef void(*megaGcmPostFunc)(void*);
    extern megaGcmPostFunc megaPostMessageToGui;
    #define _MEGA_GCM_MAKESYM(prefix, suffix) prefix##suffix
    #define MEGA_GCM_MAKESYM(prefix, suffix) _MEGA_GCM_MAKESYM(prefix, suffix)
/** This function is defined in all dynamic link libraries that want to use the gui
 * call marshaller \c megaPostMessageToGui() function via a pointer passed to an init
 * function, rather than dynamically linking to the function in the main app executable.
 * This technique can be used if dynamic linking does not work or is problematic.
 * @returns On success returns 0, otherwise an error code.
*/
    int MEGA_GCM_EXPORT MEGA_GCM_MAKESYM(megaGcmInit_, MEGA_GCM_CLIENT_INIT_API)(megaGcmPostFunc postFunc);
#endif

/** When the application's main (GUI) thread receives a message posted by
 * megaPostMessageToGui(), it must forward the \c void* pointer
 * from that message to this function for further processing. This function is
 * called by a handler in the app's (GUI) event/message loop (or equivalent, such as
 * a QT slot called on the GUI thread) and normally it should be called only
 * from one place in the code that serves as a bridge between the native message
 * loop and the mega messaging system.
* \warning Must be called only from the GUI thread
*/

static inline void megaProcessMessage(void* vptr)
{
    struct megaMessage* msg = (struct megaMessage*)vptr;
    msg->func(msg);
}

#ifdef __cplusplus
} //end extern "C"
#endif

#endif // GUICALLMARSHALLER_H
