#include "waiter/libwebsocketsWaiter.h"
#include <libwebsockets.h>

namespace mega {

LibwebsocketsWaiter::LibwebsocketsWaiter()
{
    this->wscontext = NULL;
}

LibwebsocketsWaiter::~LibwebsocketsWaiter()
{

}

void LibwebsocketsWaiter::init(dstime ds)
{
    Waiter::init(ds);
}

int LibwebsocketsWaiter::wait()
{
    assert(wscontext);
    lws_service(wscontext, ds == NEVER ? NEVER : ds * 100);
    return NEEDEXEC;
}

void LibwebsocketsWaiter::notify()
{
    assert(wscontext);
    lws_cancel_service(wscontext);
}
} // namespace
