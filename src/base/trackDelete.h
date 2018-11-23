#ifndef TRACKDELETE_H
#define TRACKDELETE_H
#include <atomic>
#include <thread>

namespace karere
{
/** @brief Used to keep track of deletion of a lambda-captured object
  * pointer/reference - the instance may get deleted before the lambda is called
  * e.g. an attribute is fetched
  */

#ifdef NDEBUG
class ThreadChecker
{
public:
    void rememberParentThread() noexcept {}
    void checkThread() const noexcept {}
};
#else
class ThreadChecker
{
    std::atomic<std::thread::id> mParentThread = {};

public:

    void rememberParentThread() noexcept
    {
        mParentThread.store(std::this_thread::get_id(), std::memory_order_release);
    }

    void checkThread() const noexcept
    {
        const auto parentThreaId = mParentThread.load(std::memory_order_acquire);
        assert(parentThreaId == std::thread::id{}
            || parentThreaId == std::this_thread::get_id());
    }
};
#endif

// This class is a type erased version of following generic WeakReferenceable<T>
// due to it has no information about the type of a parent object you can't access
// parent object using DeleteTrackable::Handle. There is no other differences
// between this class and WeakReferenceable. You can think about this class as
// a WeakReferenceable<void*> specialization including all the limitations.
class DeleteTrackable
{
    class SharedData : ThreadChecker
    {
        // mDeleted is not designed to be accessed other then from parent thread
        // so no synchronization is provided
        bool mDeleted = false;


    public:

        // no copy/move semantics
        SharedData() = default;
        SharedData(const SharedData&) = delete;
        SharedData& operator =(const SharedData&) = delete;
        ~SharedData() = default;

        bool isDeleted() const noexcept
        {
            checkThread();
            return mDeleted;
        }

        void setDeleted() noexcept
        {
            // calling this function more than once may be an issue
            assert(!mDeleted);
            rememberParentThread();
            mDeleted = true;
        }
    };

    using SharedDataPtr = std::shared_ptr<SharedData>;


public:

    class Handle
    {
        SharedDataPtr mData;


    public:

        // immutable semantic
        Handle& operator =(const Handle&) = delete;
        Handle() = delete;

        ~Handle() = default;
        Handle(const Handle&) = default;
        Handle(const SharedDataPtr& data) noexcept
            : mData(data) {}

        bool deleted() const noexcept
        {
            return mData->isDeleted();
        }
        void throwIfDeleted() const
        {
            if (deleted())
                throw std::runtime_error("TrackDelete: Instance has been deleted");
        }
    };


private:

    SharedDataPtr mSharedDataHandle = std::make_shared<SharedData>();


protected:

    DeleteTrackable() = default;
    ~DeleteTrackable()
    {
        mSharedDataHandle->setDeleted();
    }


public:

    // If you think that weakHandle returns something with weak semantic
    // and this method return something with ownership semantics then you are wrong.
    // Both methods returns a handle which provide WEAK semantics only.
    // That means parent object may destroy itself regardless of having live handles.
    Handle getDelTracker() const noexcept { return mSharedDataHandle; }
    Handle weakHandle() const noexcept { return getDelTracker(); }
};

// ATTENTION the implementation should be considered as NOT thread aware
// apart from one corner case.
//
// This class provides kind of std::enable_shared_from_this semantics
// but it doesn't require object of Derived class being allocated in the heap.
// weakHandle() is an analogue of the enable_shared_from_this::weak_from_this();
// conceptually WeakRefHandle is kind of std::weak_ptr. 
//
// WeakRefHandle provides very similar semantics with the very important exception
// when you call WeakRefHandle::weakPtr() the analogue of the weak_ptr::lock() you
// will get a weak RAW pointer on a parent object instead of an owning pointer like
// std::shared_ptr. That means you have no guarantee that the parent object alive.
// The only guarantee you have is if you get nullptr then parent object is dead.
// On the other hand, using these classes within SINGLE(parent) thread only,
// there is no way that parent object can destroy itself while other users accessing
// it via raw pointer got from WeakRefHandle::weakPtr()
//
// There is a use case when WeakRefHandle object is passed to another worker thread
// and back to parent thread without accessing members in the worker thread.
// This is OK because shared_ptr provides such thread safety guarantee.
// Original implementation handles this case by using atomic refCounter.
// Again just to repeat because this is important
// It is OK to execute copy ctor of WeakRefHandle in MULTI threaded env
// but EVERYTHING else SINGLE(parent) threaded only.
//
// Additionally, as the only exception, everything may be called in main thread
// during APP initialization and shutdown while basically there is no parent thread
// at the moment. It is not easy (if possible) to implement automatic parent thread
// migration, so be aware that there is no thread checks in DEBUG for WeakReferenceable
// ctor and weakHandle method. There is a check though in dtor and it doesn't fail
// during application shutdown either because all objects destroyed within parent thread
// or because of memory leaks but that is not critical because this is APP shutdown.
template <class T>
class WeakReferenceable
{
    class WeakRefSharedData : ThreadChecker
    {
        // mPtr is not designed to be accessed other then from parent thread
        // so no synchronization is provided
        T* mPtr;


    public:

        // no copy/movable semantics
        WeakRefSharedData(const WeakRefSharedData&) = delete;
        WeakRefSharedData& operator =(const WeakRefSharedData&) = delete;

        ~WeakRefSharedData() = default;
        WeakRefSharedData(T* ptr)
            : mPtr(ptr) {}

        T* get() const noexcept
        {
            // All the attempts, reading value which was never modified after initialization,
            // is technically OK from ANY thread. If a thread doesn't have the value in its
            // context it will read it from memory. checkThread is conceptually a NOP until
            // the very first (and supposed to be the only one) mutation of mPtr is happened.
            // However it is error prone in general to access get() method other than from
            // parent thread only.
            checkThread();
            return mPtr;
        }
        void reset() noexcept
        {
            // calling this function more than once may be an issue
            assert(mPtr);
            // Since this moment any access to get() method is CHECKED to make sure
            // that it happens from the SAME thread that reset mPtr value only.
            // It is not guaranteed for any other thread this change being seen.
            rememberParentThread();
            mPtr = nullptr;
        }
    };
    using WeakRefSharedDataPtr = std::shared_ptr<WeakRefSharedData>;


public:

    class WeakRefHandle
    {
        WeakRefSharedDataPtr mData;


    public:

        ~WeakRefHandle() = default;
        WeakRefHandle() = default;
        WeakRefHandle(const WeakRefHandle& other) = default;
        WeakRefHandle& operator=(const WeakRefHandle& other) = default;
        WeakRefHandle(const WeakRefSharedDataPtr& data) noexcept
            : mData(data) {}

        bool deleted() const noexcept { return !isValid(); }
        bool isValid() const noexcept { return mData && mData->get(); }
        void reset() noexcept { mData.reset(); }
        T* weakPtr() noexcept { return mData ? mData->get() : nullptr; }
        const T* weakPtr() const noexcept { return mData ? mData->get() : nullptr; }
        static WeakRefHandle invalid() { return WeakRefHandle(); }

        void throwIfInvalid() const
        {
            if (!isValid())
                throw std::runtime_error("WeakRefHandle::isValid: Handle is invalid or referenced object has been deleted");
        }
        T& operator*()
        {
            throwIfInvalid();
            return *mData->get();
        }
        const T& operator*() const
        {
            throwIfInvalid();
            return *mData->get();
        }
        T* operator->()
        {
            throwIfInvalid();
            return mData->get();
        }
        const T* operator->() const
        {
            throwIfInvalid();
            return mData->get();
        }
    };


private:

    WeakRefSharedDataPtr mWeakRefHandle;


protected:

    WeakReferenceable(T* target)
        : mWeakRefHandle{ std::make_shared<WeakRefSharedData>(target) }{}
    ~WeakReferenceable() { mWeakRefHandle->reset(); }


public:

    WeakRefHandle weakHandle() const noexcept { return mWeakRefHandle; }
};
}

#endif
