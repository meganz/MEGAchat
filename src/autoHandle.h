#pragma once

template <typename T, typename CF, CF CloseFunc, T InvalidValue>
class MyAutoHandle
{
protected:
    T mHandle;
public:
    inline MyAutoHandle(T handle): mHandle(handle){};
    ~MyAutoHandle()
    {
        if (mHandle != InvalidValue)
            CloseFunc(mHandle);
    }
    inline MyAutoHandle():mHandle(InvalidValue){};
    inline void free()
    {
        if (mHandle == InvalidValue)
            return;
        CloseFunc(mHandle);
        mHandle = InvalidValue;
    }
    inline operator T() const {return mHandle;}
    inline T handle() const {return mHandle;}
    inline MyAutoHandle& assign(T handle)
    {
        if (mHandle != InvalidValue)
            CloseFunc(mHandle);
        mHandle = handle;
        return *this;
    }
    inline MyAutoHandle& operator=(T handle)
    { return assign(handle); }

    inline operator bool() const {return mHandle != InvalidValue;}
private:
    MyAutoHandle& operator=(const MyAutoHandle& other); //disable copy ctor
};

