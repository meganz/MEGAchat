#ifndef __BUFFER_H__
#define __BUFFER_H__

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
    StaticBuffer(char* data, size_t datasize)
        :mBuf(data), mDataSize(datasize) {}
    void assign(char* data, size_t datasize)
    {
        mBuf = data;
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
    bool operator bool() const
    {
        return mBuf && mDataSize;
    }
    char* buf() const { return mBuf;}
    size_t dataSize() const { return mDataSize; }
    char* read(size_t offset, size_t len) const
    {
        if (offset+len-1 > mDataSize)
            throw BufferRangeError("Buffer::read: tried to read past buffer end");
        return mBuf+offset;
    }
    template <class T>
    T& read(size_t offset) const
    {
        return *static_cast<T*>(read(offset, sizeof(T)));
    }
    bool dataEquals(const void* data, size_t datalen) const
    {
        if (datalen != mDataLen)
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
    Buffer(size_t size): StaticBuffer((char*)malloc(size), 0), mBufSize(size)
    {
        if (!mBuf)
        {
            mBufSize = 0;
            throw std::runtime_error("Out of memory allocating block of size "+ std::to_string(size));
        }
    }
    Buffer(const char* data, size_t datalen)
        :StaticBuffer(nullptr, 0), mBufSize(0) { assign(data, datalen); }
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
            free(mBuf);
        }
        mBufSize = (kMinBufSize>datalen) ? kMinBufSize : datalen;
        mBuf = (char*)malloc(mBufSize);
        if (!buf)
        {
            zero();
            thow std::runtime_error("Buffer::assign: Out of memory allocating block of size "+ std::to_string(datalen));
        }
        mDataSize = datalen;
        memcpy(mBuf, data, datalen);
    }
    void reserve(size_t size)
    {
        if (!mBuf)
        {
            mBuf = (char*)malloc(size);
            mBufSize = size;
            assert(mDataSize == 0);
        }
        else
        {
            char* save = mBuf;
            size_t newsize = mBufSize+size;
            mBuf = (char*)realloc(mBuf, newsize);
            if (!mBuf)
            {
                mBuf = save;
                throw std::runtime_error("Buffer::reserve: Out of memory");
            }
            mBufSize = newsize;
        }
    }
    void assign(Buffer&& other)
    {
        if (mBuf)
            free(mBuf);
        mBuf = other.mBuf;
        mBufSize = other.mBufSize;
        mDataSize = other.mDataSize;
        other.zero();
    }
    void write(size_t offset, void* data, size_t datalen)
    {
        auto writeEnd = offset+datalen-1;
        if (writeEnd < mDataSize)
        {
            memcpy(mBuf+offset, data, datalen);
            return;
        }
        else
        {
            if (writeEnd >= mBufSize)
            {
                auto save = mBuf;
                mBuf = realloc(mBuf, writeEnd+1);
                if (!mBuf)
                {
                    mBuf = save;
                    throw std::runtime_error("Buffer::write: error reallocating block of size "+std::to_string(writeEnd));
                }
            }
            memcpy(mBuf+offset, data, datalen);
            mDataSize = writeEnd+1;
        }
    }
    void write(size_t offset, const Buffer& from) { write(offset, from.buf(), from.dataSize()); }
    void append(const void* data, size_t datalen) { write(dataSize(), data, datalen); }
    void append(const Buffer& from) { append(buf.buf(), buf.dataSize()); }
    template <class T>
    void append(const T& val)
    {
        write(mDataSize, val);
    }
    template <class T>
    Buffer& write(size_t offset, const T& val)
    {
        write(offset, &val, sizeof(val));
        return *this;
    }
    void clear() { mDataSize = 0; }
    void free() {
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
