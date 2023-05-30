#ifndef MEGACHAT_WAIT_CLASS
#define MEGACHAT_WAIT_CLASS LibuvWaiter

#include "mega/waiter.h"

#include <uv.h>

namespace mega {
struct LibuvWaiter : public Waiter
{
    LibuvWaiter();
    ~LibuvWaiter() override;

    void init(dstime) override;
    int wait() override;

    void notify() override;

    uv_loop_t* eventloop() const { return evtloop.get(); }

private:
    std::unique_ptr<uv_loop_t> evtloop;
    std::unique_ptr<uv_async_t> asynchandle;
};
} // namespace

#endif
