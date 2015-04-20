#ifndef RETRYHANDLER_H
#define RETRYHANDLER_H

#include <promise.h>
#include <base/gcm.h>
#include <karereCommon.h>

#define RETRY_DEBUG_LOGGING 1

#ifdef RETRY_DEBUG_LOGGING
    #define RETRY_LOG(fmtString,...) SVCS_LOG_DEBUG("Retry: " fmtString, ##__VA_ARGS__)
#else
    #define RETRY_LOG(fmtString,...)
#endif

namespace mega
{

template<class Func>
class RetryHandler
{
public:
    typedef typename decltype(std::declval<Func>().operator()())::Type RetType;
    enum { kPromiseType = 0x2e7294d1 };
    typedef enum
    {
        kStateNotStarted = 0,
        kStateInProgress,
        kStateFinished
    } State;

    enum
    {
        kDefaultMaxAttemptCount = 0,
        kDefaultMaxSingleWaitTime = 60000
    };

protected:
    enum { kBitness = sizeof(size_t)*8 }; //needed to determine if a retry attempt can have an exponential wait that fits in a size_t
    State mState = kStateNotStarted;
    Func mFunc;
    size_t mCurrentAttemptNo = 0;
    size_t mMaxAttemptCount;
    size_t mMaxSingleWaitTime;
    promise::Promise<RetType> mPromise;
    unsigned long mWaitTimer = 0;
    bool mAutoDestruct = false; //used when we use this object on the heap
public:
    promise::Promise<RetType>& getPromise() {return mPromise;}
    void setAutoDestroy() { mAutoDestruct = true; }
    State state() const { return mState; }
    RetryHandler(Func&& func, size_t maxSingleWaitTime=kDefaultMaxSingleWaitTime,
        size_t maxAttemptCount=kDefaultMaxAttemptCount)
    :mFunc(std::forward<Func>(func)), mMaxAttemptCount(maxAttemptCount),
      mMaxSingleWaitTime((maxSingleWaitTime>0) ? maxSingleWaitTime : ((size_t)-1))
    {}
    ~RetryHandler()
    {
        RETRY_LOG("Deleting RetryHandler instance");
    }
    void start()
    {
        if (mState != kStateNotStarted)
            throw std::runtime_error("RetryHandler: Already started or not reset after finished");
        nextTry();
    }
    bool abort()
    {
        if (mState != kStateInProgress)
            return false;

        if (mWaitTimer)
        {
            cancelTimeout(mWaitTimer);
            mWaitTimer = 0;
        }
        mPromise.reject(promise::Error("aborted", "1", kPromiseType));
        if (mAutoDestruct)
            delete this;
        return true;
    }
    void reset()
    {
        if (mState == kStateNotStarted)
            return;
        else if (mState == kStateInProgress)
            throw std::runtime_error("RetryHandler::reset: Can't reset while in progress");

        assert(mState == kStateFinished);
        asert(mWaitTimer == 0);

        mPromise = promise::Promise<RetType>();
        mCurrentAttemptNo = 0;
        mState = kStateNotStarted;
    }
    void restart()
    {
        abort();
        reset();
        start();
    }
protected:
    size_t calcWaitTime()
    {
        if (mCurrentAttemptNo > kBitness)
            return mMaxSingleWaitTime;
        size_t t = (1 << (mCurrentAttemptNo-1)) * 1000;
        printf("mMaxSingleWaitTime = %zu\n", mMaxSingleWaitTime);
        if (t <= mMaxSingleWaitTime)
            return t;
        else
            return mMaxSingleWaitTime;
    }
    void nextTry()
    {
        assert(mWaitTimer == 0);
        assert(!mPromise.done());

        mCurrentAttemptNo++;
        mFunc()
        .then([this](const RetType& ret)
        {
            mPromise.resolve(ret);
            mState = kStateFinished;
            if (mAutoDestruct)
                delete this;
            return ret;
        })
        .fail([this](const promise::Error& err)
        {
            if (mMaxAttemptCount && (mCurrentAttemptNo >= mMaxAttemptCount)) //give up
            {
                mPromise.reject(err);
                mState = kStateFinished;
                if (mAutoDestruct)
                    delete this;
                return err;
            }
            size_t waitTime = calcWaitTime();
            RETRY_LOG("Attempt %zu failed, will retry in %zu ms", mCurrentAttemptNo, waitTime);
            mWaitTimer = setTimeout([this]()
            {
                mWaitTimer = 0;
                nextTry();
            }, waitTime);
            return err;
        });
    }
};

/**
 *Convenience function to retry a lambda call returning a promise.
 *Internally it instantiates a RetryHandler instance and managed its lifetime.
 * @param The promise-returning (lambda) function to call. This function must take
 * no arguments.
 * @param maxSingleWaitTime - the maximum time in [ms] to wait between attempts. Default is 30 sec
 * @param maxRetries - the maximum number of attempts between giving up and rejecting
 * the returned promise. If it is zero, then it will retry forever. Default is 0
 */
template <class Func>
static inline auto retry(Func&& func,
    size_t maxRetries=RetryHandler<Func>::kDefaultMaxAttemptCount,
    size_t maxSingleWaitTime=RetryHandler<Func>::kDefaultMaxSingleWaitTime)
->decltype(func())
{
    auto self = new RetryHandler<Func>(
        std::forward<Func>(func), maxSingleWaitTime, maxRetries);
    auto promise = self->getPromise();
    self->setAutoDestroy();
    self->start(); //self may get destroyed synchronously here, but we have a reference to the promise
    return promise;
}

}

#endif // RETRYHANDLER_H

