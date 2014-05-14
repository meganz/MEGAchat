#ifndef ASYNCTEST_H
#define ASYNCTEST_H

#include <vector>
#include <map>
#include <mutex>
#include <thread>
#ifdef __MINGW32__
//std::mutex does not work currently under mingw
	#include <windows.h>
#endif

template <class M>
class Unlocker
{
protected:
	M& mLock;
public:
	Unlocker(M& lock):mLock(lock) {
		mLock.unlock();
	}
	~Unlocker() {
		mLock.lock();
	}
};


class Async
{
protected:
#ifdef __MINGW32__
//std::mutex does not work currently under mingw
 class Mutex
 {
  protected:
	CRITICAL_SECTION mCritSect;
  public:
	Mutex(){ InitializeCriticalSection(&mCritSect);	}
	~Mutex(){ LeaveCriticalSection(&mCritSect); }
	inline void lock() {EnterCriticalSection(&mCritSect);}
	inline void unlock() {LeaveCriticalSection(&mCritSect);}
 };
	static inline long long getTimeMs()
	{
		static Mutex mutex;
		static long long lastTicks = 0;
	  mutex.lock();
		long long ticks = GetTickCount() | (lastTicks & 0xFFFFFFFF00000000); //wraps every ~49 days, but is good enough for our purposes
		if (ticks < lastTicks)
			ticks |= ((ticks >> 32)+1)<<32; //increment more left 32bit word by 1
		lastTicks = ticks;
	  mutex.unlock();
		return ticks;
	}
	static inline void sleep(int ms)
	{	::Sleep(ms);	}

#else
	typedef std::mutex Mutex;
	static inline long long getTimeMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	static inline void sleep(int ms)
	{	std::this_thread::sleep_for(std::chrono::milliseconds(ms));	}
#endif

typedef Unlocker<Mutex> MutexUnlocker;
public:
	enum
	{
		SCHED_IN_ORDER = 0,
		ASYNC_COMPLETE_NOT = 0,
		ASYNC_COMPLETE_SUCCESS = 1,
		ASYNC_COMPLETE_ERROR = 2
	};
protected:
	struct DoneCondition
	{
		int complete = 0;
		long long deadline = 0;
		int order = 0;
	};

	int mSeqCtr = 0;
	long long mLastOrderTs = 0;
public:
	int defaultJitter = 400;
	int jitterMin = 100;
	int sleepGranularity = 50;
protected:
	bool mSingleDone = true;
	std::map<long long, std::function<void()> > mSchedQueue;
	std::map<std::string, DoneCondition> mDones;
	int mComplete = 0;
	int mWaitCount = 0;
	int doneTimeout = 2000;
	std::string mErrorMsg;
	std::string mErrorTag;
	Mutex mMutex;
	int mFlags = 0;
private:
	Async(); //we don't want lambdas to make a copy of the async object by accident
public:
	Async(const std::vector<std::string>& items)
	{
		mMutex.lock();
		mSingleDone = false;
		for (const auto& item: items)
			addDone(item);
	}

	Async(const std::map<std::string, std::map<std::string, int> >& items)
	{
		mMutex.lock();
		mSingleDone = false;
		for (const auto& item: items)
			addDone(item);
	}
	template <class S>
	void addDone(const S& userSpec)
	{
		const std::string& tag = getDoneTagFromUserSpec(userSpec);
		if (!mDones.insert(make_pair(tag, getDoneCondFromUserSpec(userSpec))).second)
			throwAndReport("Duplicate done() tag '"+tag+"'");
		mWaitCount = mDones.size();
	}
	~Async()
	{
		mMutex.unlock();
	}
	virtual void onCompleteError()
	{
		if (!mErrorTag.empty())
			throw std::runtime_error("Condition '"+mErrorTag+"': Error: "+mErrorMsg);
		else
			throw std::runtime_error("Error: "+mErrorMsg);
	}
	virtual void throwAndReport(const std::string& msg)
	{
		printf("Error calling Async API: %s\n", msg.c_str());
		throw std::runtime_error(msg);
	}

	void schedCall(std::function<void()>&& func, int after=SCHED_IN_ORDER, int jitter = 0)
	{
		if (jitter == 0)
			jitter = defaultJitter;
		long long key;
		if (after == SCHED_IN_ORDER)
		{
			long long time = mLastOrderTs+(rand()%jitter)+jitterMin;
			mLastOrderTs = time;
			key = (time << 16)|(mSeqCtr++);
		}
		else
			key = (getTimeMs()+(rand()%jitter)+jitterMin) << 16;
		std::function<void()> test = func;
		mSchedQueue[key] = func;
	}
	int run(int doneTimeout = 2000)
	{
		long long now = getTimeMs();
		for (auto& done: mDones)
		{
			auto& deadline = done.second.deadline;
			if (!deadline)
				deadline = doneTimeout;
			deadline+=now;
		}

		while(!mComplete)
		{
			while (mSchedQueue.empty())
			{
				checkTimeouts();
				if (mComplete)
					break;
				{
					MutexUnlocker unlock(mMutex);
					sleep(sleepGranularity);
				}
			}
			if (mComplete)
				break;
			long long now = getTimeMs();
			auto it = mSchedQueue.begin();
			long long timeToSleep = (it->first>>16) - now;
			auto call = it->second;
			mSchedQueue.erase(it);
			while (timeToSleep > 0)
			{
				long long start = getTimeMs(); //measure actual time, because mutex lock may block for a while
				{
					MutexUnlocker unlock(mMutex);
					sleep(sleepGranularity);
				}
				timeToSleep-=(getTimeMs()-start);
				checkTimeouts();
			}
			try
			{
				call();
			}
			catch(std::exception& e)
			{
				doError("Exception: "+(e.what() ? std::string(e.what()) : std::string("(Empty message)")), "");
			}
			catch(...)
			{
				doError("Non-standard exception", "");
			}
		}
		if (mComplete == ASYNC_COMPLETE_ERROR)
			onCompleteError();
		return mComplete;
	}
	void done(const std::string& tag)
	{
		if (mSingleDone)
			throwAndReport("done() called with a tag, but no named conditions are registered (working in single done() mode)");

		auto it = mDones.find(tag);
		if (it == mDones.end())
		{
			doError("Unknown check tag '"+tag+"'", "");
			return;
		}
		if (it->second.complete)
		{
			doError("Check already passed", tag);
			return;
		}

		mWaitCount--;
		auto order = it->second.order;
		if (order && (order != ((int)mDones.size()-mWaitCount)))
		{
			doError("Condition did not complete in expected order. Expected: "+
			 std::to_string(order)+"; actual: "+
			 std::to_string(mDones.size()-mWaitCount), it->first);
			return;
		}

		it->second.complete = ASYNC_COMPLETE_SUCCESS;

		if (mWaitCount <= 0)
		{
			assert(mWaitCount == 0);
			mComplete = ASYNC_COMPLETE_SUCCESS;
		}
	}
	void done()
	{
		if (!mSingleDone)
			throwAndReport("Called plain done() when multiple done conditions are tracked");
		mComplete = ASYNC_COMPLETE_SUCCESS;
	}
	void doError(const std::string& msg, const std::string& tag)
	{
		mErrorMsg = msg;
		mErrorTag = tag;
		mComplete = ASYNC_COMPLETE_ERROR;
		if (!tag.empty())
		{
			auto it = mDones.find(tag);
			if (it == mDones.end())
				throwAndReport("error() called with unknown tag");
			it->second.complete = ASYNC_COMPLETE_ERROR;
		}

	}
	void error(const std::string& msg)
	{
		if (!mSingleDone)
			throwAndReport("Called plain error() when multiple done conditions are tracked");
		doError(msg, "");
	}

	void error(const std::string& tag, const std::string& msg)
	{
		if (mSingleDone)
			throwAndReport("error() called with a tag, but no multiple done()s are tracked");
		doError(msg, "");

	}
	void checkTimeouts()
	{
		long long now = getTimeMs();
		for (const auto& item: mDones)
		  if (!item.second.complete && (now > item.second.deadline))
			{
				doError("Timed out waiting for condition to be satisfied", item.first);
				break;
			}
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
	template <class S>
	const std::string& getDoneTagFromUserSpec(const S& spec)
	{	return spec.first;  }

//	template <>
	const std::string& getDoneTagFromUserSpec(const std::string& spec)
	{	return spec;  }

	template <class S>
	DoneCondition getDoneCondFromUserSpec(const S& spec)
	{
		DoneCondition ret;
		auto& vals = spec.second;
		auto val = vals.find("timeout");
		if (val != vals.end())
			ret.deadline = val->second;
		val = vals.find("order");
		if (val != vals.end())
			ret.order = val->second;
		return ret;
	}

	DoneCondition getDoneCondFromUserSpec(const std::string& spec)
	{	return DoneCondition();	}

};

#endif // ASYNCTEST_H
