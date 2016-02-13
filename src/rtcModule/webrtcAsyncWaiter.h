#ifndef _WEBRTC_ASYNC_WAITER
#define _WEBRTC_ASYNC_WAITER
#include <condition_variable>
#include <mutex>
#ifdef __APPLE__
    #include <webrtc/base/scoped_autorelease_pool.h>
#endif

#ifdef RTCM_DEBUG_ASYNC_WAITER
    #define ASYNCWAITER_LOG_DEBUG(fmtString,...) KR_LOG_DEBUG("AsyncWaiter: " fmtString ": ", __##VA_ARGS__)
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
    std::atomic<size_t> mWakeUpCtr = {0};
    std::atomic<size_t> mWaitCtr = {0};
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
    ASYNCWAITER_LOG_DEBUG("Wait() called by %s thread with timeout %d",
        (rtc::Thread::Current() == mThread) ? "the GUI" : "a worker", waitTimeout);
    if (waitTimeout == 0)
    {
        return false;
    }
    else
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (waitTimeout == kForever)
        {
            //raise(SIGTRAP);
            mCondVar.wait(lock, [this]() { return mWaitCtr != mWakeUpCtr; });
        }
        else
        {
            //GUI thread should call Wait() only with timeout 0 (when polling for new
            //messages, via Get() in our processMessages() below) or with kForever (when
            //waiting for a Send() to another thread to complete, which should not take
            //long)
            KR_LOG_WARNING("Wait() called by %s thread with nonzero timeout."
                "If called by the GUI thread, GUI may freeze",
                (rtc::Thread::Current() == mThread) ? "the GUI" : "a worker");
            mCondVar.wait_for(lock, std::chrono::milliseconds(waitTimeout),
                [this]() { return mWaitCtr != mWakeUpCtr; });
        }
        mWaitCtr++;
        ASYNCWAITER_LOG_DEBUG("Wait(): Woken up, returning");
        return true;
    }
}

// Causes the current wait (if one is in progress) to wake up.
virtual void WakeUp()
{
    ASYNCWAITER_LOG_DEBUG("WakeUp() called by %s thread", (rtc::Thread::Current() == mThread)?"the GUI":"a worker");
    if (!mMessageQueue->empty()) //process messages and wake up waiters again
    {
        ASYNCWAITER_LOG_DEBUG("  WakeUp(): Message queue not empty, posting processMessages() call on GUI thread");
        mega::marshallCall([this]()
        {
            if (processMessages())
            { //signal once again that we have messages processed
                mWakeUpCtr++;
                mCondVar.notify_all();
            }
            else
            {
                ASYNCWAITER_LOG_DEBUG("  WakeUp: GUI thread: No messages in queue");
            }
        });
    }
    //If the GUI thread is waiting, we must wake it up to process messages if any.
    //If it processes any messages, it will signal the condvar once again
    mWakeUpCtr++;
    mCondVar.notify_all();
}
bool processMessages()
{
    bool hadMsg = false;
    while (true)
    {
    #if __has_feature(objc_arc)
        @autoreleasepool
    #elif defined(WEBRTC_MAC)
      // see: http://developer.apple.com/library/mac/#documentation/Cocoa/Reference/Foundation/Classes/NSAutoreleasePool_Class/Reference/Reference.html
      // Each thread is supposed to have an autorelease pool. Also for event loops
      // like this, autorelease pool needs to be created and drained/released
      // for each cycle.
        rtc::ScopedAutoreleasePool pool;
    #endif
        rtc::Message msg;
        if (!mMessageQueue->Get(&msg, 0)) //calls Wait() on us, which returns immediately
            return hadMsg;
        hadMsg = true;
        ASYNCWAITER_LOG_DEBUG("AsyncWaiter: Dispatching webrtc message on GUI thread");
        mMessageQueue->Dispatch(&msg);
    }
}
};

}
#endif
