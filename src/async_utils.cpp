
#include "async_utils.h"

namespace megachat::async
{

int ResultHandler::waitForResult(int seconds)
{
    if (std::future_status::ready != futureResult.wait_for(std::chrono::seconds(seconds)))
    {
        errorStr = "Timeout";
        return -999; // local timeout
    }
    return futureResult.get();
}

void ResultHandler::finish(int errCode, std::string&& errStr)
{
    assert(!resultReceived); // call this function only once!
    errorStr.swap(errStr);
    resultReceived = true;
    promiseResult.set_value(errCode);
}

bool waitForResponse(std::function<bool()> mustExit, unsigned int timeout_secs)
{
    if (!mustExit)
    {
        return false;
    }
    timeout_secs *= 1000000; // convert to micro-seconds
    unsigned int tWaited = 0;    // microseconds
    unsigned int pollingT = 500000;
    while(!mustExit())
    {
        std::this_thread::sleep_for(std::chrono::microseconds(pollingT));
        tWaited += pollingT;
        if (tWaited >= timeout_secs)
        {
            return false;   // timeout is expired
        }
    }
    return true;    // response is received
}


}
