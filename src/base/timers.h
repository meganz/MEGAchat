#ifndef _MEGA_BASE_TIMERS_INCLUDED
#define _MEGA_BASE_TIMERS_INCLUDED

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
    typedef void(*DestroyFunc)(TimerMsg*);
    event* timerEvent = nullptr;
    bool canceled = false;
    DestroyFunc destroy;
    megaHandle handle;
    TimerMsg(megaMessageFunc aFunc, DestroyFunc aDestroy)
        : megaMessage(aFunc), destroy(aDestroy),
          handle(services_hstore_add_handle(MEGA_HTYPE_TIMER, this))
    {}
   ~TimerMsg()
    {
        services_hstore_remove_handle(MEGA_HTYPE_TIMER, handle);
        if (timerEvent)
            event_free(timerEvent);
    }
};

template <int persist, class CB>
inline megaHandle setTimer(CB&& callback, unsigned time)
{
    struct Msg: public TimerMsg
    {
        CB cb;
        Msg(CB&& aCb, megaMessageFunc cFunc)
        :TimerMsg(cFunc, [](TimerMsg* m)
          {
            delete static_cast<Msg*>(m);
          }), cb(aCb)
        {}
    };

    Msg* pMsg = new Msg(std::forward<CB>(callback),
        persist
        ? [](megaMessage* arg)
          {
              auto msg = static_cast<Msg*>(arg);
              if (msg->canceled)
                  return;
              msg->cb();
          }
        : [](megaMessage* arg)
          {
              if (static_cast<Msg*>(arg)->canceled)
                  return;
              std::unique_ptr<Msg> msg(static_cast<Msg*>(arg));
              msg->cb();
          });
    pMsg->timerEvent = event_new(services_get_event_loop(), -1, persist,
      [](evutil_socket_t fd, short what, void* evarg)
      {
            megaPostMessageToGui(evarg);
      }, pMsg);

    struct timeval tv;
    tv.tv_sec = time / 1000;
    tv.tv_usec = (time % 1000)*1000;
    evtimer_add(pMsg->timerEvent, &tv);
    return pMsg->handle;
}

static inline bool cancelTimeout(megaHandle handle)
{
    assert(handle);
    TimerMsg* timer = static_cast<TimerMsg*>(
                services_hstore_get_handle(MEGA_HTYPE_TIMER, handle));
    if (!timer)
        return false; //not valid anymore

//we have to make sure that we delete the timer only after all possibly queued
//timer messages in the app's message queue are processed. For this purpose,
//we first stop the timer, and only then post a call to delete the timer.
//That call should be processed after all timer messages
    assert(timer->timerEvent);
    assert(timer->destroy);
    timer->canceled = true; //disable timer callback being called by possibly queued messages, and message freeing in one-shot timer handler
    event_del(timer->timerEvent); //only removed from message loop, doesn't delete the event struct
    marshallCall([timer]()
    {
        timer->destroy(timer); //also deletes the timerEvent
    });
    return true;
}

static inline bool cancelInterval(megaHandle handle)
{
    return cancelTimeout(handle);
}

template<class CB>
static inline megaHandle setTimeout(CB&& cb, unsigned time)
{
    return setTimer<0>(std::forward<CB>(cb), time);
}

template <class CB>
static inline megaHandle setInterval(CB&& callback, unsigned time)
{
    return setTimer<EV_PERSIST>(std::forward<CB>(callback), time);
}

}
#endif
