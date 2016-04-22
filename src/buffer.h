#ifndef __BUFFER_H__
#define __BUFFER_H__
#include <stdlib.h>
#include <assert.h>
#include <stdexcept>
#include <string.h>

class BufferRangeError: public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class StaticBuffer
{
protected:
    char* mBuf;
    size_t mDataSize;
public:
    StaticBuffer(const char* data, size_t datasize)
        :mBuf((char*)data), mDataSize(datasize) {}
    void assign(const char* data, size_t datasize)
    {
        mBuf = (char*)data;
        mDataSize = datasize;
    }
    void clear()
    {
        mBuf = nullptr;
        mDataSize = 0;
    }
    bool empty() const
    {
        return !(mBuf && mDataSize);
    }
    operator bool() const
    {
        return mBuf && mDataSize;
    }
    const char* buf() const { return mBuf;}
    size_t dataSize() const { return mDataSize; }
    char* read(size_t offset, size_t len) const
    {
        if (offset+len > mDataSize)
            throw BufferRangeError("Buffer::read: tried to read "+
                std::to_string(offset+len-mDataSize)+" bytes past buffer end");
        return mBuf+offset;
    }
    template <class T>
    T& read(size_t offset) const
    {
        return *((T*)(read(offset, sizeof(T))));
    }
    bool dataEquals(const void* data, size_t datalen) const
    {
        if (datalen != mDataSize)
            return false;
        return (memcmp(mBuf, data, datalen) == 0);
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
    Buffer(size_t size=kMinBufSize): StaticBuffer((char*)malloc(size), 0), mBufSize(size)
    {
        if (!mBuf)
        {
            mBufSize = 0;
            throw std::runtime_error("Out of memory allocating block of size "+ std::to_string(size));
        }
    }
    Buffer(const char* data, size_t datalen)
        :StaticBuffer(nullptr, 0), mBufSize(0) { if (data) assign(data, datalen); }
    Buffer(Buffer&& other)
        :StaticBuffer(other.mBuf, other.mDataSize), mBufSize(other.mBufSize) { other.zero(); }
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
        mBufSize = (kMinBufSize>datalen) ? kMinBufSize : datalen;
        mBuf = (char*)malloc(mBufSize);
        if (!mBuf)
        {
            zero();
            throw std::runtime_error("Buffer::assign: Out of memory allocating block of size "+ std::to_string(datalen));
        }
        mDataSize = datalen;
        ::memcpy(mBuf, data, datalen);
    }
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
            char* save = mBuf;
            size_t newsize = mBufSize+size;
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
    void assign(Buffer&& other)
    {
        if (mBuf)
            ::free(mBuf);
        mBuf = other.mBuf;
        mBufSize = other.mBufSize;
        mDataSize = other.mDataSize;
        other.zero();
    }
    Buffer& write(size_t offset, const void* data, size_t datalen)
    {
        if (data == 0)
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
            }
            memcpy(mBuf+offset, data, datalen);
            mDataSize = reqdSize;
        }
        return *this;
    }
    Buffer& write(size_t offset, const Buffer& from) { return write(offset, from.buf(), from.dataSize()); }
    Buffer& write(size_t offset, const std::string& str) { return write(offset, str.c_str(), str.size()); }
    Buffer& append(const void* data, size_t datalen) { return write(dataSize(), data, datalen);}
    Buffer& append(const Buffer& from) { return append(from.buf(), from.dataSize());}
    template <class T>
    Buffer& append(const T& val) { return write(mDataSize, val);}
    Buffer& append(const char* str) { return append((void*)str, strlen(str)); }
    template <class T>
    Buffer& write(size_t offset, const T& val) { return write(offset, &val, sizeof(val)); }
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
