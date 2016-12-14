#ifndef TRACKDELETE_H
#define TRACKDELETE_H

namespace karere
{
/** @brief Used to keep track of deletion of a lambda-captured object
  * pointer/reference - the instance may get deleted before the lambda is called
  * e.g. an attribute is fetched
  */
class DeleteTrackable
{
public:
    struct SharedData
    {
        int32_t mRefCount = 0;
    };
    class Handle
    {
    protected:
        SharedData* mData;
        Handle(SharedData* shared)
        : mData(shared) { mData->mRefCount+=2; }
        Handle& operator=(Handle& other) = delete;
    public:
        Handle(const Handle& other): Handle(other.mData){}
        ~Handle()
        {
            auto& cnt = mData->mRefCount;
            cnt-=2;
            if (cnt <= 1)
            {
                assert(cnt >= 0);
                delete mData;
            }
        }
        bool deleted() const { return mData->mRefCount & 0x01; }
        void throwIfDeleted() const
        {
            if (deleted())
                throw std::runtime_error("TrackDelete: Instance has been deleted");
        }
        friend class DeleteTrackable;
    };

protected:
    Handle mSharedDataHandle;
public:
    Handle getDelTracker() const { return Handle(mSharedDataHandle.mData); }
    Handle weakHandle() const { return getDelTracker(); }
    DeleteTrackable(): mSharedDataHandle(new SharedData()){}
    ~DeleteTrackable() { mSharedDataHandle.mData->mRefCount |= 0x01; }
};


template <class T>
class WeakReferenceable
{
private:
    WeakReferenceable(const WeakReferenceable& other) = delete;
protected:
    template<typename I, typename = void>
    struct is_iterator
    {
       static constexpr bool value = false;
    };

    template<typename I>
    struct is_iterator<I, typename std::enable_if<!std::is_same<typename std::iterator_traits<T>::value_type, void>::value>::type>
    {
       static constexpr bool value = true;
    };
public:
    struct WeakRefSharedData
    {
        T mPtr;
        int32_t mRefCount = 0;
        WeakRefSharedData(T aPtr): mPtr(aPtr){}
    };

    class WeakRefHandle
    {
    protected:
        WeakRefSharedData* mData;
        WeakRefHandle(WeakRefSharedData* data)
        : mData(data)
        {
            ref();
        }
        void ref()
        {
            if (mData)
                mData->mRefCount += 2;
        }
        void unref()
        {
            if (!mData)
                return;
            auto& cnt = mData->mRefCount;
            cnt-=2;
            if (cnt <= 1)
            {
                assert(cnt >= 0);
                delete mData;
            }
        }
    public:
        friend class WeakReferenceable;
        WeakRefHandle(): mData(nullptr) {}
        WeakRefHandle(const WeakRefHandle& other): WeakRefHandle(other.mData){}
        WeakRefHandle& operator=(const WeakRefHandle& other)
        {
            unref();
            mData = other.mData;
            ref();
            return *this;
        }
        ~WeakRefHandle() { unref(); }
        bool isValid() const { return (mData && ((mData->mRefCount & 0x01) == 0)); }
        void reset()
        {
            unref();
            mData = nullptr;
        }
        void throwIfInvalid() const
        {
            if (!isValid())
                throw std::runtime_error("WeakRefHandle::isValid: Handle is invalid or referenced object has been deleted");
        }
        static WeakRefHandle invalid() { return WeakRefHandle(); }
        decltype(*(mData->mPtr))& operator*()
        {
            throwIfInvalid();
            return *mData->mPtr;
        }
        const decltype(*(mData->mPtr))& operator*() const
        {
            throwIfInvalid();
            return *mData->mPtr;
        }
        T weakPtr()
        {
            return mData ? mData->mPtr : T();
        }
        const T weakPtr() const
        {
            return mData ? mData->mPtr : T();
        }
        T operator->() { return weakPtr(); }
        const T operator->() const { return weakPtr(); }
    };
protected:
    WeakRefHandle mWeakRefHandle;
public:
    //T is a pointer
    template <class C=T, class=typename std::enable_if<std::is_pointer<C>::value>::type>
    WeakReferenceable(T target): mWeakRefHandle(new WeakRefSharedData(target)){}
    //T is iterator
    template <class C=T, class=typename std::enable_if<is_iterator<C>::value>::type>
    WeakReferenceable(): mWeakRefHandle(new WeakRefSharedData(T())){}

    ~WeakReferenceable()
    {
        assert(mWeakRefHandle.isValid());
        mWeakRefHandle.mData->mRefCount |= 0x01;
    }
    WeakRefHandle getWeakHandle() const { return mWeakRefHandle; }

    /** @brief
     * If the instance reference is an iterator, it is usually set after construction,
     * when the item is inserted in the container. So we need to provide
     * a method to set it after construction.
     */
    template <class C=T, class=typename std::enable_if<is_iterator<C>::value>::type>
    void setIterator(T target)
    {
        mWeakRefHandle.mData->mPtr = target;
    }
};
}

#endif
