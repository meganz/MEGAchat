#ifndef _MEGA_BASE_TIMERS_INCLUDED
#define _MEGA_BASE_TIMERS_INCLUDED
/**
 * @file timers.h
 * @brief C++11 asynchronous timer header-only lib. Provides a timer API similar
 * to that of javascript
 *
 * (c) 2013-2015 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */
#include "cservices.h"
#include "gcmpp.h"
#include <memory>
#include <assert.h>

namespace karere
{

struct TimerMsg: public megaMessage
{
    timerevent* timerEvent = nullptr;
    bool canceled = false;
    megaHandle handle;
    TimerMsg(megaMessageFunc aFunc)
        :megaMessage(aFunc),
          handle(services_hstore_add_handle(MEGA_HTYPE_TIMER, this))
    {}
   ~TimerMsg()
    {
        services_hstore_remove_handle(MEGA_HTYPE_TIMER, handle);
        if (timerEvent)
        {            
            uv_close((uv_handle_t *)timerEvent, [](uv_handle_t* handle)
            {
                delete handle;
            });
        }
    }
};

void init_uv_timer(void *ctx, uv_timer_t *timer);

extern std::recursive_mutex timerMutex;

template <int persist, class CB>
inline megaHandle setTimer(CB&& callback, unsigned time, void *ctx)
{
    struct Msg: public TimerMsg
    {
        CB cb;
        void *appCtx;
        Msg(CB&& aCb, megaMessageFunc cFunc)
        :TimerMsg(cFunc), cb(aCb)
        {}
        unsigned time;
        int loop;
    };
    megaMessageFunc cfunc = persist
        ? (megaMessageFunc) [](void* arg)
          {
              Msg* msg = static_cast<Msg*>(arg);
              if (msg->canceled)
                  return;
              msg->cb();
          }
        : (megaMessageFunc) [](void* arg)
          {
              timerMutex.lock();
              Msg* msg = static_cast<Msg*>(arg);
              if (msg->canceled)
              {
                  timerMutex.unlock();
                  return;
              }
              msg->cb();
              if (msg->canceled)
              {
                  timerMutex.unlock();
                  return;
              }
              delete msg;
              timerMutex.unlock();
          };

    timerMutex.lock();
    Msg* pMsg = new Msg(std::forward<CB>(callback), cfunc);
    timerMutex.unlock();

    pMsg->appCtx = ctx;
    pMsg->time = time;
    pMsg->loop = persist;  
    marshallCall([pMsg, ctx]()
    {
        pMsg->timerEvent = new uv_timer_t();
        pMsg->timerEvent->data = pMsg;
        init_uv_timer(ctx, pMsg->timerEvent);
        uv_timer_start(pMsg->timerEvent,
                       [](uv_timer_t* handle)
                       {
                           megaPostMessageToGui(handle->data, ((Msg*)handle->data)->appCtx);
                       }, pMsg->time, pMsg->loop ? pMsg->time : 0);
    }, ctx);    
    return pMsg->handle;
}
/** Cancels a previously set timeout with setTimeout()
 * @return \c false if the handle is not valid. This can happen if the timeout
 * already triggered, then the handle is invalidated. This situation is safe and
 * considered normal
 */
static inline bool cancelTimeout(megaHandle handle, void *ctx)
{
    timerMutex.lock();

    assert(handle);
    TimerMsg* timer = static_cast<TimerMsg*>(services_hstore_get_handle(MEGA_HTYPE_TIMER, handle));
    if (!timer)
    {
        timerMutex.unlock();
        return false; //not valid anymore
    }

//we have to make sure that we delete the timer only after all possibly queued
//timer messages in the app's message queue are processed. For this purpose,
//we first stop the timer, and only then post a call to delete the timer.
//That call should be processed after all timer messages
    timer->canceled = true; //disable timer callback being called by possibly queued messages, and message freeing in one-shot timer handler
    timerMutex.unlock();    
    marshallCall([timer, ctx]()
    {
        uv_timer_stop(timer->timerEvent);

        marshallCall([timer, ctx]()
        {
            delete timer;
        }, ctx);
    }, ctx);
    return true;
}
/** @brief Cancels a previously set timer with setInterval.
 * @return \c false if the handle is not valid.
 */
static inline bool cancelInterval(megaHandle handle, void *ctx)
{
    return cancelTimeout(handle, ctx);
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
static inline megaHandle setTimeout(CB&& cb, unsigned timeMs, void *ctx)
{
    return setTimer<0>(std::forward<CB>(cb), timeMs, ctx);
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
static inline megaHandle setInterval(CB&& callback, unsigned timeMs, void *ctx)
{
    return setTimer<0x10>(std::forward<CB>(callback), timeMs, ctx);
}

}
#endif
