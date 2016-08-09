#ifndef _MEGA_GCMPP_INCLUDED
#define _MEGA_GCMPP_INCLUDED

/* C++11 bindings to the GUI call marashaller mechanism */

#include "gcm.h"
#include <memory>
#include <assert.h>

namespace karere
{
/** This function uses the plain C Gui Call Marshaller mechanism (see gcm.h) to
 * marshal a C++11 lambda function call on the main (GUI) thread. Also it could
 * be used with a std::function or any other object with operator()). It provides
 * type safety since it generates both the message type and the code that processes
 * it. Further, it allows for code optimization as all types are known at compile time
 * and all code is in the same compilation unit, so it can be inlined
 */
template <class F>
static inline void marshallCall(F&& func)
{
    struct Msg: public megaMessage
    {
        F mFunc;
        Msg(F&& aFunc, megaMessageFunc cHandler)
        : megaMessage(cHandler), mFunc(std::forward<F>(aFunc)){}
#ifndef NDEBUG
        unsigned magic = 0x3e9a3591;
#endif
    };
// Ensure that the message is deleted even if exception is thrown in the lambda.
// Although an exception should not happen here and will propagate to the
// application's message/event loop. TODO: maybe provide a try/catch block here?
// Asses the performence impact of this
// We use a custom-tailored smart ptr here to gain some performance (i.e. destructor
// always deletes, no null check needed)
    struct AutoDel
    {
        Msg* mMsg;
        AutoDel(Msg* aMsg): mMsg(aMsg){}
        ~AutoDel() { delete this->mMsg; }
        Msg* operator->() { return this->mMsg; }
    };
    Msg* msg = new Msg(std::forward<F>(func),
    [](void* ptr)
    {
        AutoDel pMsg(static_cast<Msg*>(ptr));
        assert(pMsg->magic == 0x3e9a3591);
        pMsg->mFunc();
    });
    megaPostMessageToGui(static_cast<void*>(msg));
}

}
#endif
