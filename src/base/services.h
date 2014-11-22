#ifndef _MEGA_BASE_SERVICES_INCLUDED
#define _MEGA_BASE_SERVICES_INCLUDED

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

template <class CB>
static inline void* setTimeout(CB callback, int time)
{
    //TODO: Implement
}
static inline void cancelTimeout(void* handle)
{
    //TODO: implement
}

template <class CB>
static void* setInterval(CB callback, int time)
{
    //TODO: Implement
}
static inline void cancelInterval(void* handle)
{
    //TODO: Implement
}

}
#endif
