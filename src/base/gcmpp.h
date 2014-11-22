#ifndef _MEGA_GCMPP_INCLUDED
#define _MEGA_GCMPP_INCLUDED

/* C++11 bindings to the GUI call marashaller mechanism */

#include "guiCallMarshaller.h"
#include <memory>
#include <assert.h>

namespace mega
{
template <class F>
static inline void marshallCall(F&& func)
{
    struct Msg: public megaMessage
    {
        F mFunc;
        Msg(F&& aFunc, megaMessageFunc cHandler): megaMessage(cHandler), mFunc(aFunc){}
#ifndef NDEBUG
        unsigned magic = 0x3e9a3591;
#endif
    };
    Msg* msg = new Msg(std::forward<F>(func),
    [](megaMessage* ptr)
    {
        std::unique_ptr<Msg> pMsg(static_cast<Msg*>(ptr));
        assert(pMsg->magic == 0x3e9a3591);
        pMsg->mFunc();
    });
    megaPostMessageToGui(static_cast<void*>(msg));
}

}
#endif
