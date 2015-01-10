#ifndef _MEGA_BASE_TIMERS_INCLUDED
#define _MEGA_BASE_TIMERS_INCLUDED

#include "cservices.h"
#include "gcmpp.h"
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
    :megaMessage(aFunc), destroy(aDestroy),
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
/** Cancels a previously set timeout with setTimeout()
 * @return \c false if the handle is not valid. This can happen if the timeout
 * already triggered, then the handle is invalidated. This situation is safe and
 * considered normal
 */
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
/** @brief Cancels a previously set timer with setInterval.
 * @return \c false if the handle is not valid.
 */
static inline bool cancelInterval(megaHandle handle)
{
    return cancelTimeout(handle);
}
/**
 *
 *@brief Sets a one-shot timer, similar to javascript's setTimeout()
 *@param cb This is a C++11 lambda, std::function or any other object that
 * has \c operator()
 * @param timeMs - the time in milliseconds after which the callback
 * will be called one single time and the timer will be destroyed,
 * and the handle will be invalidated
 * @returns a handle that can be used to cancel the timeout
 */
template<class CB>
static inline megaHandle setTimeout(CB&& cb, unsigned timeMs)
{
    return setTimer<0>(std::forward<CB>(cb), time);
}
/**
 @brief Sets a repeating timer, similar to javascript's setInterval
 @param callback A C++11 lambda function, std::function or any other object
 that has \c operator()
 @param timeMs - the timer's period in milliseconds. The function will be called
 releatedly until cancelInterval() is called on the returned handle
 @returns a handle that can be used to cancel the timer
*/
template <class CB>
static inline megaHandle setInterval(CB&& callback, unsigned timeMs)
{
    return setTimer<EV_PERSIST>(std::forward<CB>(callback), time);
}

}
#endif
