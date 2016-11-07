#ifndef TRACKDELETE_H
#define TRACKDELETE_H

namespace karere
{
/** @brief Used to keep track of deletion of a lambda-captured object
  * pointer/reference - the instance may get deleted before the lambda is called
  * e.g. an attribute is fetched
  */
class TrackDelete
{
public:
    struct SharedData
    {
        bool mDeleted = false;
        uint32_t mRefCount = 0;
    };
    class Handle
    {
    protected:
        SharedData* mData;
        Handle(SharedData* shared)
        : mData(shared) { mData->mRefCount++; }
        Handle& operator=(Handle& other) = delete;
    public:
        Handle(const Handle& other): Handle(other.mData){}
        ~Handle()
        {
            if (--(mData->mRefCount) <= 0)
                delete mData;
        }
        bool deleted() const { return mData->mDeleted; }
        void throwIfDeleted() const
        {
            if (mData->mDeleted)
                throw std::runtime_error("TrackDelete: Instance has been deleted");
        }
        friend class TrackDelete;
    };

protected:
    Handle mSharedDataHandle;
public:
    Handle getWeakPtr() const { return Handle(mSharedDataHandle.mData); }
    TrackDelete(): mSharedDataHandle(new SharedData()){}
    ~TrackDelete() { mSharedDataHandle.mData->mDeleted = true; }
};
}

#endif
