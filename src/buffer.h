#ifndef __BUFFER_H__
#define __BUFFER_H__
#include <assert.h>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <vector>

#if !defined(__arm__) && !defined(__aarch64__)
    #define BUFFER_ALLOW_UNALIGNED_MEMORY_ACCESS 1
#endif
class BufferRangeError: public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class StaticBuffer
{
protected:
    char* mBuf;
    size_t mDataSize;
    StaticBuffer() {} //used by Buffer to skip initialization
public:
    static const size_t kNotFound = {(size_t)-1};
    StaticBuffer(const void* data, size_t datasize)
        :mBuf((char*)data), mDataSize(datasize) {}

    //optionally include the terminating NULL
    StaticBuffer(const std::string& str, bool termNull)
      : StaticBuffer(str.c_str(), termNull?(str.size()+1):str.size()){}

    void assign(const void* data, size_t datasize)
    {
        mBuf = (char*)data;
        mDataSize = datasize;
    }
    void clear() { mDataSize = 0; }
    void setDataSize(size_t newsize)
    {
        if (newsize > mDataSize)
            throw std::runtime_error("Can't increase data size of static buffer - no info about buffer capacity");
        mDataSize = newsize;
    }
    bool empty() const { return !(mBuf && mDataSize); }
    operator bool() const { return mBuf && mDataSize;    }
    char* buf() const { return mBuf; }
    unsigned char* ubuf() const { return reinterpret_cast<unsigned char*>(mBuf); }
    template<typename T>
    T* typedBuf() const { return reinterpret_cast<T*>(mBuf); }

    size_t dataSize() const { return mDataSize; }
    size_t size() const { return mDataSize; } //alias to dataSize()
    void checkDataSize(size_t size) const
    {
        if (mDataSize < size)
            throw BufferRangeError(
                "StaticBuffer::ensureDataSize: Data size "
                +std::to_string(mDataSize) + " is less then the minimum expected "
                +std::to_string(size));
    }
    char* readPtr(size_t offset, size_t len) const
    {
        if (offset+len > mDataSize)
            throw BufferRangeError("Buffer::read: tried to read "+
                std::to_string(offset+len-mDataSize)+" bytes past buffer end");
        return mBuf+offset;
    }
    template <class T>
    T read(size_t offset) const
    {
        return alignSafeRead<T>(readPtr(offset, sizeof(T)));
    }
    template <typename T>
    static T alignSafeRead(const void* ptr)
    {
#ifndef BUFFER_ALLOW_UNALIGNED_MEMORY_ACCESS
        T val;
        memcpy(&val, ptr, sizeof(T));
        return val;
#else
        return *((T*)ptr);
#endif
    }
    template <class T>
    void read(size_t offset, std::vector<T>& output, int count)
    {
        T* end = (T*)(mBuf+offset+count*sizeof(T));
        for (T* pitem = (T*)(mBuf+offset); pitem < end; pitem++)
        {
            output.push_back(alignSafeRead<T>(pitem));
        }
    }
    template <class T>
    void read(size_t offset, std::vector<T>& output)
    {
        assert((mDataSize-offset) % sizeof(T) == 0);
        size_t count = (mDataSize-offset)/sizeof(T);
        read(offset, output, static_cast<int>(count));
    }
    template <class T>
    void read(size_t offset, T& output)
    {
        assert(offset+sizeof(T) <= mDataSize);
        memcpy(&output, mBuf+offset, sizeof(T));
    }
    void read(size_t offset, size_t len, std::string& output)
    {
        assert(offset+len <= mDataSize);
        output.assign(mBuf + offset, len);
    }
    bool dataEquals(const void* data, size_t datalen) const
    {
        if (datalen != mDataSize)
            return false;
        return (memcmp(mBuf, data, datalen) == 0);
    }
    bool dataEquals(const StaticBuffer& other)
    {
        return dataEquals(other.buf(), other.dataSize());
    }
    size_t find(unsigned char val, size_t offset=0)
    {
        for (size_t i=offset; i<mDataSize; i++)
            if (mBuf[i] == val)
                return i;
        return kNotFound;
    }
    std::string toString(unsigned colCount=47) const
    {
        static const char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
        if (!mDataSize)
            return "(empty)";
        auto hexCount = mDataSize*3;
        std::string result;
        result.reserve(16+hexCount+hexCount/colCount+2);
        result+="size: ";
        result.append(std::to_string(mDataSize))+='\n';
        unsigned colCtr = 0;
        for (size_t i=0;;)
        {
            unsigned char val = reinterpret_cast<unsigned char*>(mBuf)[i];
            result+=hexChars[val >> 4];
            result+=hexChars[val & 0x0f];
            i++;
            if (i >= mDataSize)
                break;
            colCtr+=2;
            if (colCtr >= colCount)
            {
                colCtr = 0;
                result+='\n';
            }
            else
            {
                colCtr++;
                result+=' ';
            }
        }
        return result;
    }
};

class Buffer: public StaticBuffer
{
protected:
    size_t mBufSize;
    enum {kMinBufSize = 64};
    void zero()
    {
        mBuf = nullptr;
        mBufSize = 0;
        mDataSize = 0;
    }
public:
    char* buf() { return mBuf;}
    const char* buf() const { return mBuf;}
    size_t bufSize() const { return mBufSize;}
    const char* c_str() const
    {
        if (empty())
            return nullptr;

        char *ret = new char[size() + 1];
        memcpy(ret, buf(), size());
        ret[size()] = '\0';
        return ret;
    }
    Buffer(size_t size=kMinBufSize, size_t dataSize=0)
    {
        assert(dataSize <= size);
        if (size)
        {
            mBuf = (char*)malloc(size);
            if (!mBuf)
            {
                zero();
                throw std::runtime_error("Out of memory allocating block of size "+ std::to_string(size));
            }
            mBufSize = size;
            mDataSize = dataSize;
        }
        else
        {
            zero();
        }
    }
    Buffer(const char* data, size_t datalen)
    {
        if (data && datalen)
        {
            mBuf = (char*)malloc(datalen);
            mBufSize = datalen;
            memcpy(mBuf, data, datalen);
            mDataSize = datalen;
        }
        else
        {
            zero();
        }
    }
    Buffer(Buffer&& other)
        :StaticBuffer(other.mBuf, other.mDataSize), mBufSize(other.mBufSize) { other.zero(); }

    template <bool withNull>
    Buffer(const std::string& src)
    {
        mBufSize = withNull ? src.size()+1 : src.size();
        mBuf = (char*)malloc(mBufSize);
        memcpy(mBuf, src.c_str(), mBufSize);
        mDataSize = mBufSize;
    }
    void assign(const void* data, size_t datalen)
    {
        if (mBuf)
        {
            if (datalen <= mBufSize)
            {
                memcpy(mBuf, data, datalen);
                mDataSize = datalen;
                return;
            }
            ::free(mBuf);
        }
        mBufSize = (kMinBufSize > datalen) ? (size_t) kMinBufSize : datalen;
        mBuf = (char*)malloc(mBufSize);
        if (!mBuf)
        {
            zero();
            throw std::runtime_error("Buffer::assign: Out of memory allocating block of size "+ std::to_string(datalen));
        }
        mDataSize = datalen;
        ::memcpy(mBuf, data, datalen);
    }
    template <bool withNull>
    void assign(const std::string& src) { assign(src.c_str(), withNull?(src.size()+1):src.size()); }
    void copyFrom(const StaticBuffer& src) { assign(src.buf(), src.dataSize()); }
    void reserve(size_t size)
    {
        if (!mBuf)
        {
            mBuf = (char*)::malloc(size);
            mBufSize = size;
            assert(mDataSize == 0);
        }
        else
        {
            size_t newsize = mDataSize+size;
            if (newsize <= mBufSize)
                return;
            char* save = mBuf;
            mBuf = (char*)::realloc(mBuf, newsize);
            if (!mBuf)
            {
                mBuf = save;
                throw std::runtime_error("Buffer::reserve: Out of memory");
            }
            mBufSize = newsize;
        }
    }
    void setDataSize(size_t size)
    {
        if (size > mBufSize)
            throw std::runtime_error("setDataSize: Attempted to set dataSize to span beyond bufferSize");
        mDataSize = size;
    }
    char* writePtr(size_t offset, size_t dataLen)
    {
        auto reqdSize = offset+dataLen;
        if (reqdSize > mBufSize)
        {
            reserve(reqdSize);
            mDataSize = reqdSize;
        }
        else if (reqdSize > mDataSize)
        {
            mDataSize = reqdSize;
        }
        return mBuf+offset;
    }
    char* appendPtr(size_t dataLen) { return writePtr(mDataSize, dataLen); }
    void assign(const StaticBuffer& other)
    {
        assign(other.buf(), other.dataSize());
    }
    Buffer& write(size_t offset, const void* data, size_t datalen)
    {
        if (!data)
            return *this;
        auto reqdSize = offset+datalen;
        if (reqdSize <= mDataSize)
        {
            ::memcpy(mBuf+offset, data, datalen);
            return *this;
        }
        else
        {
            if (reqdSize > mBufSize)
            {
                auto save = mBuf;
                mBuf = (char*)::realloc(mBuf, reqdSize);
                if (!mBuf)
                {
                    mBuf = save;
                    throw std::runtime_error("Buffer::write: error reallocating block of size "+std::to_string(reqdSize));
                }
                mBufSize = reqdSize;
            }
            memcpy(mBuf+offset, data, datalen);
            mDataSize = reqdSize;
        }
        return *this;
    }
    Buffer& write(size_t offset, const StaticBuffer& from) { return write(offset, from.buf(), from.dataSize()); }
    Buffer& write(size_t offset, const std::string& str) { return write(offset, str.c_str(), str.size()); }
    Buffer& append(const void* data, size_t datalen) { return write(dataSize(), data, datalen);}
    Buffer& append(const std::string& str) { return write(dataSize(), str.c_str(), str.size()); }
    Buffer& append(const StaticBuffer& from) { return append(from.buf(), from.dataSize());}
    template <class T, typename=typename std::enable_if<std::is_pod<T>::value && !std::is_pointer<T>::value>::type>
    Buffer& append(T val) { return write(mDataSize, val);}
    Buffer& append(const char* str) { return append((void*)str, strlen(str)); }
    template <class T, typename=typename std::enable_if<std::is_pod<T>::value && !std::is_pointer<T>::value>::type>
    Buffer& write(size_t offset, const T& val) { return write(offset, &val, sizeof(val)); }
    template <typename T>
    T& mapRef(size_t offset) { return *reinterpret_cast<T*>(writePtr(offset, sizeof(T))); }
    void fill(size_t offset, uint8_t value, size_t count)
    {
        memset(writePtr(offset, count), value, count);
    }
    void appendFill(uint8_t value, size_t count)
    {
        memset(appendPtr(count), value, count);
    }
    void clear() { mDataSize = 0; }
    void free()
    {
        if (!mBuf)
            return;
        ::free(mBuf);
        mBuf = nullptr;
        mBufSize = mDataSize = 0;
    }

    ~Buffer()
    {
        if (mBuf)
            ::free(mBuf);
    }
};
#endif
