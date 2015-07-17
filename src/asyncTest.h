#ifndef TESTLOOP_H
#define TESTLOOP_H

#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <string.h> //for strcmp
#include <assert.h>
#include <unistd.h>

/** default timeout for a done() item */
#ifndef TESTLOOP_DEFAULT_DONE_TIMEOUT
    #define TESTLOOP_DEFAULT_DONE_TIMEOUT 2000
#endif

#define TESTLOOP_LOG(fmtString,...) printf("TESTLOOP: " fmtString "\n", ##__VA_ARGS__)
#define TESTLOOP_LOG_ERROR(fmtString,...) TESTLOOP_LOG("%sERR: " fmtString "%s", kColorFail, ##__VA_ARGS__, kColorNormal)

#ifdef TESTLOOP_VERBOSE
    #define TESTLOOP_LOG_VERBOSE(fmtString,...) TESTLOOP_LOG(fmtString, ##__VA_ARGS__)
#else
    #define TESTLOOP_LOG_VERBOSE(fmtString,...)
#endif

#ifdef TESTLOOP_LOG_DONES
    #define TESTLOOP_LOG_DONE(fmtString,...) TESTLOOP_LOG(fmtString, ##__VA_ARGS__)
#else
    #define TESTLOOP_LOG_DONE(fmtString,...)
#endif

#ifdef TESTLOOP_DEBUG
    #define TESTLOOP_LOG_DEBUG(fmtString,...) TESTLOOP_LOG(fmtString, ##__VA_ARGS__)
#else
    #define TESTLOOP_LOG_DEBUG(fmtString,...)
#endif

template <class M>
class Unlocker
{
protected:
	M& mLock;
public:
    Unlocker(M& lock):mLock(lock) {  mLock.unlock();	}
    ~Unlocker() { mLock.lock(); }
};

/** An async execution loop that runs scheduled function calls, added via schedCall(),
 * and watches for user-specified 'conditions', added via addDone() being resolved
 * within the specified timeout
 */
class EventLoop
{
protected:
    typedef long long Ts;
    static inline Ts getTimeMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	static inline void sleep(int ms)
	{	std::this_thread::sleep_for(std::chrono::milliseconds(ms));	}

typedef Unlocker<std::mutex> MutexUnlocker;
public:
	enum
	{
		SCHED_IN_ORDER = 0,
		ASYNC_COMPLETE_NOT = 0,
		ASYNC_COMPLETE_SUCCESS = 1,
        ASYNC_COMPLETE_ERROR = 2,
        ASYNC_COMPLETE_ABORTED = 3
	};
    enum
    {
        kEventTypeUnknown = 0,
        kEventTypeDone = 1,
        kEventTypeSchedCall = 2
    };
protected:
/**A done() item (added by addDone()) that has to be resolved by the user code
 * withing a specified timeout and/or order, related to other such items
*/
    struct DoneItem
	{
		int complete = 0;
        Ts deadline = TESTLOOP_DEFAULT_DONE_TIMEOUT;
        int order = 0;
        std::string tag;
        DoneItem(const char* aTag): tag(aTag){}
        DoneItem(const char* aTag, const char* name1, int val1)
        :tag(aTag) { setVal(name1, val1); }
        DoneItem(const char* aTag, const char* name1, int val1, const char* name2, int val2)
        :tag(aTag)
        {
            setVal(name1, val1);
            setVal(name2, val2);
        }
        DoneItem(const DoneItem&) = default;
        DoneItem(DoneItem&& other)
        :tag(std::move(other.tag)), complete(other.complete), deadline(other.deadline),
          order(other.order){}
        void setVal(const char* name, int val)
        {
            if ((strcmp(name, "timeout") == 0) || (strcmp(name, "tmo") == 0))
                deadline = val;
            else if (strcmp(name, "order") == 0)
                order = val;
            else
                throw std::runtime_error(std::string("Unknown property '")+name+"'' of done() with tag '"+tag+"'");
        }
    };
/**A scheduled function call, added by schedCall(), that is executed by EventLoop
 * after a specified time elapses (relative to the time it was added via schedCall())
*/
    struct SchedItemBase
    {
        virtual void operator()() = 0;
        virtual ~SchedItemBase(){}
    };
    template <class CB>
    struct SchedItem: public SchedItemBase
    {
        CB mCb;
        SchedItem(CB&& cb): mCb(std::forward<CB>(cb)){}
        virtual void operator()() { mCb(); }
    };

    uint32_t mSeqCtr = 0;
    uint64_t mLastOrderTs = 0;
    int mOrderedDonesCtr = 0;
    Ts mNextEventTs = 0xFFFFFFFFFFFFFFF;
    int mResolvedDones = 0;
public:
	int defaultJitter = 400;
	int jitterMin = 100;
	int sleepGranularity = 50;
protected:
    const char* kColorSuccess = "";
    const char* kColorFail = "";
    const char* kColorNormal = "";
    const char* kColorTag = "";

/**The sched queue key has a timestamp as the most significant 32 bits and a uid counter
key as the least significant 32 bits. So it's always ordered in execution time order
*/
    typedef std::multimap<Ts, std::shared_ptr<SchedItemBase> > SchedQueue;
    SchedQueue mSchedQueue;
/**A map is of done() items, keyed by a unique tag */
    typedef std::map<std::string, DoneItem> DoneMap;
    DoneMap mDones;
    bool mHasDefaultDone = false;
/** This flag marks the end of the event loop and is set when all done() items
 * are resolved and all scheduled func calls have been executed */
	int mComplete = 0;
	std::string mErrorTag;
    std::mutex mMutex;
    void initColors()
    {
        if (!isatty(1))
            return;

        kColorSuccess = "\033[1;32m";
        kColorFail = "\033[1;31m";
        kColorNormal = "\033[0m";
        kColorTag = "\033[34m";
    }
private:
    EventLoop(const EventLoop&) = delete; //we don't want lambdas to make a copy of the async object by accident
public:
    std::string errorMsg;
    EventLoop(int timeout=TESTLOOP_DEFAULT_DONE_TIMEOUT)
    {
        DoneItem item("_default");
        item.deadline = timeout;
        addDone(std::move(item));
    }
    EventLoop(std::vector<DoneItem>&& doneItems)
    {
        mMutex.lock();
        for (auto& item: doneItems)
        {
            addDone(std::move(item));
        }
	}

    void addDone(DoneItem&& item)
	{
        if (item.tag == "_default")
            mHasDefaultDone = true;

        item.deadline += getTimeMs();
        std::string tag = item.tag; //for lambda

        auto result = mDones.insert(make_pair(item.tag, std::forward<DoneItem>(item)));
        if (!result.second)
            usageError("addDone: Duplicate done() tag '"+item.tag+"'");
        schedHandler([this,tag]()
        {
            auto it = mDones.find(tag);
            if (it == mDones.end())
            {
                doError("Internal error: done() timeout handler could not find done item"+tag, tag);
                return;
            }
            TESTLOOP_LOG_DEBUG("done() handler executed with %lld ms offset from ideal", it->second.deadline-getTimeMs());
            auto offset = abs(it->second.deadline-getTimeMs());
            if (offset > 10)
                doError("Internal error: done('"+tag+"'') timeout handle executed with time offset of "+std::to_string(offset)+" (>10ms) from required", "");
            if (it->second.complete)
                return;
            doError("Timeout", it->first, true);
        }, result.first->second.deadline);
	}
    ~EventLoop()
	{
		mMutex.unlock();
	}
	virtual void onCompleteError()
    {}
    void abort()
    {
        if (mComplete)
            return;
        mComplete = ASYNC_COMPLETE_ABORTED;
    }
    virtual void usageError(const std::string& msg)
	{
        TESTLOOP_LOG_ERROR("Usage error: %s\n", msg.c_str());
		throw std::runtime_error(msg);
	}
    template <class CB>
    void schedCall(CB&& func, int after=-10, int jitter = 0)
	{
		if (jitter == 0)
			jitter = defaultJitter;
        Ts ts;
        if (after < 0) //ordered call: schedule -after ms after the previous ordered call
		{
            if (!mLastOrderTs)
                mLastOrderTs = getTimeMs();
            ts = mLastOrderTs-after; //after is negative
            mLastOrderTs = ts;
		}
		else
        {
            ts = getTimeMs()+after;
            if (jitter)
                ts+=jitter/2-(rand()%jitter);
        }
        schedHandler(std::forward<CB>(func), ts);
    }
    template <class CB>
    void schedHandler(CB&& handler, Ts ts)
    {
        mSchedQueue.emplace(ts, std::make_shared<SchedItem<CB> >(std::forward<CB>(handler)));
        if (ts < mNextEventTs)
            setWakeupTs(ts);
	}
    void setWakeupTs(Ts& ts)
    {
        mNextEventTs = ts;
        TESTLOOP_LOG_DEBUG("Setting next event after %lld ms", ts-getTimeMs());
    }

    void run()
	{
        initColors();
        if (mSchedQueue.empty())
            throw std::runtime_error("Nothing to run: not even a single function call has been scheduled");
        while ((mSchedQueue.size() - mResolvedDones > 0) && !mComplete)
		{
            auto sched = mSchedQueue.begin();
            auto timeToSleep = sched->first - getTimeMs();
            if (timeToSleep > 0)
            {
                MutexUnlocker unlock(mMutex);
                TESTLOOP_LOG_DEBUG("Sleeping %lld ms before next event", timeToSleep);
                sleep(timeToSleep);
            }
            else
            {
                TESTLOOP_LOG_DEBUG("Negative or zero time to next event: %lld", timeToSleep);
            }
            if (sched->first - getTimeMs() > 2)
            {
                TESTLOOP_LOG_DEBUG("Woke up before next event time, will sleep again");
                continue; //slept less than required, repeat
            }
            auto call = sched->second;
            mSchedQueue.erase(sched);
            (*call)();
            if (!errorMsg.empty())
                break;
        }
        if (!mComplete) //sched queue got empty, all is done
            mComplete = ASYNC_COMPLETE_SUCCESS;
	}
	void done(const std::string& tag)
	{
		auto it = mDones.find(tag);
		if (it == mDones.end())
		{
            usageError("Unknown done() tag '"+tag+"'");
			return;
		}
		if (it->second.complete)
		{
            doError("done() already resloved, can't resolve again", tag);
			return;
		}
        mResolvedDones++; //decrement even if out of order, doesnt matter, as we are exiting the loop anyway, but for consistency
        auto order = it->second.order;
        if (order && (order != ++mOrderedDonesCtr))
		{
            doError("Did not resolve in expected order. Expected: "+
			 std::to_string(order)+"; actual: "+
             std::to_string(mOrderedDonesCtr), it->first);
			return;
		}

		it->second.complete = ASYNC_COMPLETE_SUCCESS;
        TESTLOOP_LOG_DONE("done('\%s%s\%s') -> %ssuccess%s", kColorTag, tag.c_str(),
            kColorNormal, kColorSuccess, kColorNormal);
    }

	void done()
	{
        done("_default");
	}
    void doError(const std::string& msg, const std::string& tag, bool noThrow=false)
	{
        assert(mComplete == ASYNC_COMPLETE_NOT);
		mComplete = ASYNC_COMPLETE_ERROR;
        if (!tag.empty())
        {
            auto it = mDones.find(tag);
            if (it == mDones.end())
                usageError("error() called with unknown tag: "+tag);
            mErrorTag = tag;
            errorMsg = ("done('");
            errorMsg.append(kColorTag).append(tag).append(kColorNormal)
                   .append("'): ").append(msg);
            it->second.complete = ASYNC_COMPLETE_ERROR;
            TESTLOOP_LOG_DONE("%s", errorMsg.c_str());
            if (!noThrow)
                throw std::runtime_error(errorMsg); //propagate to unit test framework to handle
        }
        else
        {
            errorMsg = msg;
            mErrorTag = tag;
            TESTLOOP_LOG_ERROR("%s", msg.c_str());
            if (!noThrow)
                throw std::runtime_error(msg);
        }
	}
	void error(const std::string& msg)
	{
        doError(msg, "_default");
    }
    void error(const std::string& tag, const std::string& msg)
    {
        if (tag.empty())
            usageError("error() for a tagged done() item called, but the tag is empty");
        doError(msg, tag);
    }

	static inline const char* completeCodeToString(int code)
	{
		static const char* strings[] =
		{
			"ASYNC_COMPLETE_NOT",
			"ASYNC_COMPLETE_SUCCESS",
			"ASYNC_COMPLETE_ERROR"
		};
		if ((code<0) || (code>(int)(sizeof(strings)/sizeof(strings[0]))))
			throw std::runtime_error("Invalid code value "+std::to_string(code));
		return strings[code];
	}
};

#endif // ASYNCTEST_H

