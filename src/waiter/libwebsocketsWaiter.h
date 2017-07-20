#ifndef MEGACHAT_WAIT_CLASS
#define MEGACHAT_WAIT_CLASS LibwebsocketsWaiter

#include <libwebsockets.h>
#include "mega/waiter.h"

namespace mega {
struct LibwebsocketsWaiter : public Waiter
{
    struct lws_context *wscontext;

    LibwebsocketsWaiter();
    ~LibwebsocketsWaiter();

    void init(dstime);
    int wait();

    void notify();

};
} // namespace

#endif
