#ifndef ICRYPTOSTRING_H
#define ICRYPTOSTRING_H
#include <string>
#include <string.h>

namespace rtcModule
{
class IString
{
public:
    virtual ~IString(){}
    virtual const char* c_str() const = 0;
    virtual bool empty() const = 0;
    virtual size_t size() const = 0;
};

class InputString
{
protected:
    const char* mStr;
    mutable size_t mSize;
public:
    InputString(const std::string& str)
        :mStr(str.c_str()), mSize(str.size())
    {}
    InputString(const char* str, size_t aSize=((size_t)-1))
        :mStr(str), mSize(aSize)
    {}
    const char* c_str() const {return mStr;}
    operator const char*() const {return mStr;}
    operator bool() const {return mStr!=nullptr;}
    size_t size() const
    {
        if (mSize != (size_t)-1)
            return mSize;
        return (mSize = strlen(mStr));
    }
};

class IString_string: public IString
{
public:
    std::string mString;
    IString_string(std::string&& str): mString(str){}
    virtual ~IString_string(){}
    virtual const char* c_str() const {return mString.c_str();}
    virtual size_t size() const {return mString.size();}
    virtual bool empty() const {return mString.empty();}
};
template<typename F=void(void*), F FreeFunc=::free>
class IString_buffer: public IString
{
protected:
    char* mBuf;
    size_t mStrSize;
    bool mHasBuf;
public:
    IString_buffer(char* buf, size_t aSize=std::string::npos)
    {
        if (!buf)
        {
            mBuf = buf;
            mHasBuf = true;
            mStrSize = aSize;
        }
        else
        {
            mBuf = "";
            mHasBuf = false;
            mStrSize = 0;
        }
    }
    void setStrSize(size_t aSize)
    {
        mStrSize = aSize;
    }
    char* bufWritePtr() const {return mBuf;}
    virtual ~IString_buffer()
    {
        if (mHasBuf)
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
    virtual bool empty() const {return mBuf[0] == 0;}
};

}
#endif // ICRYPTOSTRING_H
