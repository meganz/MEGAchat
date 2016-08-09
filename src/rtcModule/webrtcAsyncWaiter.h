#ifndef _WEBRTC_ASYNC_WAITER
#define _WEBRTC_ASYNC_WAITER
#include <condition_variable>
#include <mutex>
#ifdef __APPLE__
    #include <webrtc/base/scoped_autorelease_pool.h>
#endif

#ifdef RTCM_DEBUG_ASYNC_WAITER
    #define ASYNCWAITER_LOG_DEBUG(fmtString,...) KR_LOG_DEBUG("AsyncWaiter: " fmtString, ##__VA_ARGS__)
#else
    #define ASYNCWAITER_LOG_DEBUG(fmString,...)
#endif

namespace artc
{
/**
 * @brief The AsyncWaiter class implements processing of internal webrtc messages by
 * the GUI thread. In other words, it integrates the webrtc event loop, that webrtc
 * assigns to the GUI thread, with the application's main loop. This is done in an
 * asynchronous way, but mimics the normal behaviour of webrtc threads, so the GUI
 * thread looks to webrtc like a normal webrtc thread with a blocking message loop.
 */
class AsyncWaiter: public rtc::SocketServer
{
protected:
    std::mutex mMutex;
    std::condition_variable mCondVar;
    volatile bool mSignalled = false;
    rtc::Thread* mThread = nullptr;
    rtc::MessageQueue* mMessageQueue = nullptr;
public:
    rtc::Thread* guiThread() const { return mThread; }
    void setThread(rtc::Thread* thread) { mThread = thread; }
//rtc::SocketFactory interface
//we don't deal with any sockets, so this is a dummy impl
virtual rtc::Socket* CreateSocket(int type) { return nullptr; }
virtual rtc::Socket* CreateSocket(int family, int type) { return nullptr; }
virtual rtc::AsyncSocket* CreateAsyncSocket(int type) { return nullptr; }
virtual rtc::AsyncSocket* CreateAsyncSocket(int family, int type) {return nullptr; }
//rtc::SocketServer interface
virtual void SetMessageQueue(rtc::MessageQueue* queue) { mMessageQueue = queue; }
virtual bool Wait(int waitTimeout, bool process_io) //return false means error, this causes MessageQueue::Get() to bail out
{
// In the current webrtc implementation, Wait() on the GUI thread can be called in 2 cases:
// - by Send() from the GUI thread to wait for a completion response posted back to
// us from the worker thread. In this case the wait timeout is kForever, i.e. -1
// - By Get(), via processMessages() in our pseudo message loop - to process any
// messages that are queued on us. This is initiated when someone calls WakeUp on us,
// which is usually done when they posted a message on our queue. In this case the timeout
// is 0.
    ASYNCWAITER_LOG_DEBUG("Wait(): Called by %s thread with timeout %d",
        (rtc::Thread::Current() == mThread) ? "the GUI" : "a worker", waitTimeout);

    if (waitTimeout == 0)
        return false;

    std::unique_lock<std::mutex> lock(mMutex);
    if (waitTimeout == kForever)
    {
        while(!mSignalled)
        {
            ASYNCWAITER_LOG_DEBUG("Wait(): Waiting for signal...");
            mCondVar.wait(lock);
        }
    }
    else
    {
        KR_LOG_WARNING("Wait(): Called by %s thread with nonzero timeout."
                       "If called by the GUI thread, GUI may freeze",
                       (rtc::Thread::Current() == mThread) ? "the GUI" : "a worker");
        while (!mSignalled)
        {
            ASYNCWAITER_LOG_DEBUG("Wait(): Waiting for signal...");
            mCondVar.wait_for(lock, std::chrono::milliseconds(waitTimeout));
        }
    }
    mSignalled = false;
    ASYNCWAITER_LOG_DEBUG("Wait(): Returning");
    return true;
}

// Causes the current wait (if one is in progress) to wake up.
virtual void WakeUp()
{
    ASYNCWAITER_LOG_DEBUG("WakeUp(): Called by %s thread", (rtc::Thread::Current() == mThread)?"the GUI":"a worker");
    if (!mMessageQueue->empty()) //process messages and wake up waiters again
    {
        ASYNCWAITER_LOG_DEBUG("  WakeUp(): Message queue not empty, posting ProcessMessages(0) call on GUI thread");
        karere::marshallCall([this]()
        {
            if (mThread->ProcessMessages(0))
            { //signal once again that we have messages processed
                std::lock_guard<std::mutex> lock(mMutex);
                mSignalled = true;
                mCondVar.notify_all();
            }
            else
            {
                ASYNCWAITER_LOG_DEBUG("  WakeUp: GUI thread: No messages in queue, someone processed them before us");
            }
        });
    }
    //If the GUI thread is waiting, we must wake it up to process messages if any.
    //If it processes any messages, it will signal the condvar once again
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mSignalled = true;
        mCondVar.notify_all();
    }
}
};

}
#endif
