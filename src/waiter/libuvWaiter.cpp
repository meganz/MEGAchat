/**
 * @file posix/wait.cpp
 * @brief POSIX event/timeout handling
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
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

#include "libuvWaiter.h"

#ifdef __APPLE__
#define CLOCK_MONOTONIC 0
int clock_gettime(int, struct timespec* t)
{
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv)
    {
        return rv;
    }
    t->tv_sec = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

namespace mega {
dstime Waiter::ds;

    
static void keepalive_timer_cb(uv_timer_t* handle) { }
    
LibuvWaiter::LibuvWaiter()
{
    eventloop = new uv_loop_t();
    uv_loop_init(eventloop);
    
    uv_timer_t* timerhandle = new uv_timer_t();
    uv_timer_init(eventloop, timerhandle);
    uv_timer_start(timerhandle, keepalive_timer_cb, 1234567890ULL, 1234567890ULL);
}

LibuvWaiter::~LibuvWaiter()
{

}

void LibuvWaiter::init(dstime ds)
{
    Waiter::init(ds);
}

// update monotonously increasing timestamp in deciseconds
void Waiter::bumpds()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ds = ts.tv_sec * 10 + ts.tv_nsec / 100000000;
}

int LibuvWaiter::wait()
{
    uv_run(eventloop, UV_RUN_DEFAULT);
    return NEEDEXEC;
}

void LibuvWaiter::notify()
{
    uv_stop(eventloop);
}
} // namespace
