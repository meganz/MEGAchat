#ifndef MEGACHAT_WAIT_CLASS
#define MEGACHAT_WAIT_CLASS LibuvWaiter

#include "mega/waiter.h"

#include <uv.h>

namespace mega {
struct LibuvWaiter : public Waiter
{
    LibuvWaiter();
    ~LibuvWaiter();

    void init(dstime);
    int wait();

    void notify();
    
    uv_loop_t* eventloop;
    uv_async_t *asynchandle;
};
} // namespace

#endif
