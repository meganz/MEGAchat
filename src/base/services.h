#ifndef _MEGA_BASE_SERVICES_INCLUDED
#define _MEGA_BASE_SERVICES_INCLUDED

#include "guiCallMarshaller.h"
#include "cservices.h"
#include "gcmpp.h" //TODO: Maybe include it directly in code thet uses it
#include <event2/event.h>
#include <memory>
#include <assert.h>

namespace mega
{
struct TimerMsg: public megaMessage
{
    event* timerEvent = nullptr;
    using megaMessage::megaMessage;
    virtual ~TimerMsg()
    {
        if (timerEvent)
            event_free(timerEvent);
    }
};

template <class CB>
static inline void* setTimer(CB&& callback, int time, int persist)
{
    struct Msg: public TimerMsg
    {
        CB cb;
        Msg(CB&& aCb, megaMessageFunc cFunc)
            :TimerMsg(cFunc), cb(aCb){}
    };
    Msg* pMsg = new Msg(std::forward<CB>(callback),
        [](megaMessage* arg)
        {
            std::unique_ptr<Msg> msg(static_cast<Msg*>(arg));
            msg->cb();
        });
    //we make a copy of the event in handle because until we return pMsg->timerEvent,
    //event may have alpready fired and pMsg freed.
    event* handle = pMsg->timerEvent = event_new(services_getLibeventLoop(), -1, persist,
      [](evutil_socket_t fd, short what, void *arg)
      {
            megaPostMessageToGui(arg);
      }, pMsg);
    struct timeval tv;
    tv.tv_sec = time / 1000;
    tv.tv_usec = (time % 1000)*1000;
    evtimer_add(pMsg->timerEvent, &tv);
    return handle;
}

static inline bool cancelTimeout(void* handle)
{
    event* timer = static_cast<event*>(handle);
    if (event_del(timer) != 0)
        return false; //not valid enymore

    TimerMsg* msg = static_cast<TimerMsg*>(event_get_callback_arg(timer));
    if (msg)
        delete msg; //also deletes the event itself
    return true;
}
static inline bool cancelInterval(void* handle)
{
    return cancelTimeout(handle);
}

template<class CB>
static inline void* setTimeout(CB&& cb, int time)
{
    return setTimer(std::forward<CB>(cb), time, 0);
}

template <class CB>
static inline void* setInterval(CB&& callback, int time)
{
    return setTimer(std::forward<CB>(callback), time, EV_PERSIST);
}

}
#endif
