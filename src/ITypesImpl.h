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
}
