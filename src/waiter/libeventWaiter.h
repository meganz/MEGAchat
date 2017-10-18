#ifndef MEGACHAT_WAIT_CLASS
#define MEGACHAT_WAIT_CLASS LibeventWaiter

#include "mega/waiter.h"

#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>

namespace mega {
struct LibeventWaiter : public Waiter
{
    LibeventWaiter();
    ~LibeventWaiter();

    void init(dstime);
    int wait();

    void notify();
    
    struct event_base* eventloop;
    struct event* keepalive;
};
} // namespace

#endif
