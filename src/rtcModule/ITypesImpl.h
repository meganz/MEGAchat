#ifndef ITYPES_IMPL_H
#define ITYPES_IMPL_H
#include <string>
#include <string.h>
#include "ITypes.h"

namespace rtcModule
{

class IString_string: public IString
{
    virtual ~IString_string(){}
public:
    std::string mString;
    IString_string(std::string&& str): mString(str){}
    IString_string(const std::string& str): mString(str){}
    virtual const char* c_str() const {return mString.c_str();}
    virtual size_t size() const {return mString.size();}
    virtual bool empty() const {return mString.empty();}
};
class IString_ref: public IString
{
    virtual ~IString_ref(){}
    const std::string& mStr;
public:
    IString_ref(const std::string& str): mStr(str) {}
    virtual const char* c_str() const { return mStr.c_str(); }
    virtual size_t size() const { return mStr.size(); }
    virtual bool empty() const { return mStr.empty(); }
};

template<typename F=void(*)(void*), F FreeFunc=::free>
class IString_buffer: public IString
{
protected:
    char* mBuf;
    mutable size_t mStrSize;
public:
    IString_buffer(char* buf, size_t aSize=std::string::npos)
        :mBuf(buf), mStrSize(aSize){}
    void setStrSize(size_t aSize)
    {
        mStrSize = aSize;
        mBuf[mStrSize] = 0;
    }
    char* bufWritePtr() const {return mBuf;}
    virtual ~IString_buffer()
    {
        if (mBuf)
            FreeFunc(mBuf);
    }
    virtual const char* c_str() const {return mBuf;}
    virtual size_t size() const
    {
        if (mStrSize != std::string::npos)
            return mStrSize;
        else
            return (mStrSize = strlen(mBuf));
    }
    virtual bool empty() const {return (!mBuf || (mBuf[0] == 0));}
};

template < class T>
class IRefCountedMixin: virtual public T
{
protected:
    std::atomic<long> mRefCount;
public:
    IRefCountedMixin(): mRefCount(0){}
    virtual void addRef() { mRefCount++; }
    virtual void release()
    {
        mRefCount--;
        assert((mRefCount >= 0) || "IRefCounted: negative refCount detected, probably release() has been called without a previous addRef()");
        if (mRefCount <= 0)
            delete this;
    }
};

class IRefCountedBase
{
protected:
    std::atomic<long> mRefCount;
    virtual ~IRefCountedBase(){}
public:
    IRefCountedBase(): mRefCount(0) {}
    virtual void addRef() { mRefCount++; }
    virtual void release()
    {
        mRefCount--;
        assert((mRefCount >= 0) || "IRefCounted: negative refCount detected, probably release() has been called without a previous addRef()");
        if (mRefCount <= 0)
            destroy();
    }
    virtual void destroy()
    {
        delete this;
    }
};

}

#endif
