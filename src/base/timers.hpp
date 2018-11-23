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
extern std::mutex timerMutex;

namespace karere
{
struct TimerMsg: public megaMessage
{
    bool canceled = false;
    using megaMessage::megaMessage;
};

#ifdef USE_LIBWEBSOCKETS
    void init_uv_timer(void *ctx, uv_timer_t *timer);
#else
    eventloop *get_ev_loop(void *ctx);
#endif
    
template <int persist, class CB>
megaHandle setTimer(CB&& callback, unsigned time, void *ctx)
{
    struct Timer : TimerMsg
    {
        timerevent* timerEvent = nullptr;
        megaHandle handle = {};
        CB cb;
        void *appCtx;

        static void singleShot(void* arg)
        {
            std::unique_ptr<Timer> timer(static_cast<Timer*>(arg));
            if (!timer->isCanceled())
                timer->cb();
        }

        static void multiShot(void* arg)
        {
            const auto timer = static_cast<Timer*>(arg);
            if (!timer->isCanceled())
            {
                timer->cb();
                return;
            }
            //we have to make sure that we delete the timer only after all possibly queued
            //timer messages in the app's message queue are processed. For this purpose,
            //we first stop the timer, and only then post a call to delete the timer.
            //That call should be processed after all timer messages
#ifdef USE_LIBWEBSOCKETS
            uv_timer_stop(timer->timerEvent);
#else
            event_del(timer->timerEvent);
#endif
            marshallCall([timer] { delete timer; }, timer->appCtx);
        }

        Timer(CB&& aCb, unsigned time, void *ctx)
            : TimerMsg(persist ? multiShot : singleShot)
            , cb(std::forward<CB>(aCb))
            , appCtx(ctx)
        {
            // Not sure about following point but looks like
            // timer machinery itself is not thread safe
            // any way marshaling timer creation to the main thread
            // is correct regardless of my assumption. In worst case
            // it will add runtime overhead.
            marshallCall([this, time]()
            {
#ifndef USE_LIBWEBSOCKETS
                timerEvent = event_new(get_ev_loop(appCtx), -1, persist,
                    [](evutil_socket_t fd, short what, void* evarg)
                {
                    megaPostMessageToGui(evarg, ((Timer*)evarg)->appCtx);
                }, this);

                struct timeval tv;
                tv.tv_sec = time / 1000;
                tv.tv_usec = (time % 1000) * 1000;
                evtimer_add(pMsg->timerEvent, &tv);
#else
                timerEvent = new uv_timer_t();
                timerEvent->data = this;
                init_uv_timer(appCtx, timerEvent);
                uv_timer_start(timerEvent, [](uv_timer_t* handle)
                {
                    megaPostMessageToGui(handle->data, ((Timer*)handle->data)->appCtx);
                }, time, persist ? time : 0);
#endif
            }, appCtx);

            std::lock_guard<std::mutex> guard(timerMutex);
            handle = services_hstore_add_handle(MEGA_HTYPE_TIMER, this);
        }
        ~Timer()
        {
            {
                std::lock_guard<std::mutex> guard(timerMutex);
                services_hstore_remove_handle(MEGA_HTYPE_TIMER, handle);
            }
            if (timerEvent)
            {
#ifndef USE_LIBWEBSOCKETS
                event_free(timerEvent);
#else
                uv_close((uv_handle_t *)timerEvent, [](uv_handle_t* handle)
                {
                    delete handle;
                });
#endif
            }
        }

        bool isCanceled() const noexcept
        {
            std::lock_guard<std::mutex> guard(timerMutex);
            return canceled;
        }
    };

    return (new Timer(std::forward<CB>(callback), time, ctx))->handle;
}
/** Cancels a previously set timeout with setTimeout()
 * @return \c false if the handle is not valid. This can happen if the timeout
 * already triggered, then the handle is invalidated. This situation is safe and
 * considered normal
 */
static inline bool cancelTimeout(megaHandle handle, void*)
{
    assert(handle);
    std::lock_guard<std::mutex> guard(timerMutex);

    TimerMsg* timer = static_cast<TimerMsg*>(services_hstore_get_handle(MEGA_HTYPE_TIMER, handle));
    if (timer)
        timer->canceled = true;

    return timer != nullptr;
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
