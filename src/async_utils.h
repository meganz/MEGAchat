#pragma once
#include <atomic>
#include <future>
#include <string>
#include <assert.h>
#include <thread>

namespace megachat::async
{

static const unsigned int TEN_MINUTES_IN_SEC = 600;
static const unsigned int HALF_MINUTE_IN_SECS = 30;

class ResultHandler
{
public:
    int waitForResult(int seconds = TEN_MINUTES_IN_SEC);

    const std::string& getErrorString() const { return errorStr; }

protected:
    void finish(int errCode, std::string&& errStr);

    bool finished() const { return resultReceived; }

protected:
    std::promise<int> promiseResult;
    std::future<int> futureResult = promiseResult.get_future();
    std::atomic<bool> resultReceived = false;
    std::string errorStr;
};

bool waitForResponse(const std::function<bool()>& mustExit, unsigned int timeout_secs);

bool waitForResponse(const std::function<bool()>& mustExit,
                     const std::chrono::seconds& timeout_secs);
}
