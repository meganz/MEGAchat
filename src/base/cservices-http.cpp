#include "cservices.h"
#include "services-dns.hpp"
#include <curl/curl.h>
#include <event2/event.h>

#define always_assert(cond) \
    if (!(cond)) SVC_LOG_ERROR("HTTP: Assertion failed: '%s' at file %s, line %d", #cond, __FILE__, __LINE__)

namespace mega
{

namespace http //deserves its own namespace
{

const char* url_find_host_end(const char* p);
MEGAIO_EXPORT t_string_bounds services_http_url_get_host(const char* url);

class Buffer
{
protected:
    char* mBuf = nullptr;
    size_t mBufSize = 0;
    size_t mDataSize = 0;
public:
    char* buf() const { return mBuf; }
    ~HttpBuffer() { if (mBuf) free(mBuf); }
    size_t size() const {return size;}
    size_t dataSize() const {return mDataSize;}
    void ensureAppendSize(size_t size)
    {
        if (mBuf)
        {
            size += mDataSize;
            if (size > mSize)
                mBuf = ::realloc(mBuf, size);
            else
                return;
        }
        else
        {
            mBuf = malloc(size);
        }
        mSize = size;
    }
    const char* append(size_t writeSize)
    {
        ensureAppendSize(writeSize);
        const char* appendPtr = mBuf+mDataSize;
        mDataSize+=writeSize;
    }
    void clearData() { mDataSize = 0;}
    void shrinkToFitData(size_t maxReserve)
    {
        if (!mBuf)
            return;
        size_t maxSize = mDataSize+maxReserve;
        if (mSize > maxSize)
        {
            mBuf = realloc(mBuf, maxSize);
            mSize = maxSize;
        }
    }
};
//common base
template <class T>
class WriteAdapterBase
{
protected:
    T& mSink;
public:
    WriteAdapter(T& target): mSink(target){}
    T& sink() const {return mSink;}
};

template <>
class WriteAdapter<std::string>: public WriteAdapterBase<std::string>
{
public:
    using WriteAdapterBase::WriteAdapterBase;
    void reserve(size_t size) {mSink.reserve(size);}
    void append(const char* data, size_t len) { mSink.append(data, len); }
};

class Client;

//polymorphic base
class ResponseBase
{
protected:
    Client& mClient;
public:
    ResponseBase(Client& client): mClient(client){}
    Client& client() const {return mClient;}
    virtual ~ResponseBase(){}
};

template <class T>
class Response: public ResponseBase
{
    WriteAdapter<T> mWriter;
public:
    Response(Client& client, T& sink, size_t initialSize)
        :ResponseBase(client), mWriter(sink)
    {
        if(initialSize > 0)
            mWriter.reserve(initialSize);
    }
};

template <class T>
class ResponseWithOwnData: public Response<T>
{
public:
    std::shared_ptr<T> data;
    ResponseWithOwnData(Client &client, size_t initialSize)
        :Response<T>(client, *(new T), initialSize)
    {
//base class is always initialized first, so we need to create the mData object before we
//reach to initializing mData, and then get the pointer to it back from the base class
//we could avoid these things by multiply inheriting first from T and next from Response<T>
//but then we would have to do more exotic polymorphic casting from ResponseBase,
//because Response<T> will not be the first class in the inheritance chain, so the
//pointer to ResponseBase would have to be offset-adjusted to cast it to ResponseWithOwnData
//using dynamic_cast.
        mData.reset(&(mWriter.sink()));
    }
};

#define _curleopt(opt, val) \
    do {                                                      \
    CURLcode ret = curl_easy_setopt(mCurl, opt, val);         \
    if (ret != CURLE_OK)                                      \
        throw std::runtime_error(std::string("curl_easy_setopt(")+ #opt +") at file "+ __FILE__+ ":"+std::to_string(__LINE__)); \
    } while(0)

class HttpClient
{
protected:
    CURL* mCurl;
    int mMaxRetryWaitTime = 30;
    int mMaxRetryCount = 10;
    bool busy = false;
    curl_slist* mCustomHeaders = nullptr;
    std::unique_ptr<ResponseBase> mResponse;
    void* mReader = nullptr;
    HttpClient()
    :curl(curl_easy_init())
    {
        if (!curl)
            throw runtime_error("Could not create a CURL easy handle");
        it ret = curl_multi_add_handle(gCurlMultiHandle, curl);
        if (ret != CURLE_OK)
            throw runtime_error("Could not add CURL easy handle to multi handle");
        _curleopt(CURLOPT_PRIVATE, this);
        _curleopt(CURLOPT_USERAGENT, gHttpUserAgent.c_str());
        _curleopt(CURLOPT_FOLLOWLOCATION, 1L);
        _curleopt(CURLOPT_AUTOREFERER, 1L);
        _curleopt(CURLOPT_MAXREDIRS, 5L);
        _curleopt(CURLOPT_CONNECTTIMEOUT, 30L);
        _curleopt(CURLOPT_TIMEOUT, 20L);
        _curleopt(CURLOPT_ACCEPT_ENCODING, ""); //enable compression
        _curleopt(CURLOPT_COOKIEFILE, "");
        _curleopt(CURLOPT_COOKIESESSION, 1L);
    }
    template <class R>
    void getTo(const std::string& url, R& response)
    {
        _curleopt(CURLOPT_HTTPGET, 1);
        _curleopt(CURLOPT_WRITEDATA, this);
        auto writefunc = [](char *ptr, size_t size, size_t nmemb, void *userp)
        {
            size_t len = size*nmemb;
            static_cast<R*>(userp)->append((const char*)ptr, len);
            return len;
        };
        _curleopt(CURLOPT_WRITEFUNCTION, writefunc);
    }
    template <class CB>
    void resolveUrlDomain(const char* url, const CB& cb)
    {
        auto bounds = services_http_url_get_host(url.c_str());
        auto type = services_dns_host_type(bounds.start, bounds.end);
        if (type == SVC_DNS_HOST_DOMAIN)
        {
            string domain(bounds.start, bounds.end-bounds.start);
            dnsLookup(domain.c_str(), gIpMode,
            [this, cb](int errCode, const char* errMsg, std::shared_ptr<AddrInfo>& addrs)
            {
                if (errCode)
                {
                    cb(errCode, nullptr);
                    return;
                }

                if (cbgIpMode == SVCF_DNS_IPV4)
                {
                   if (addrs->ip4addrs().empty())
                       cb(SVCDNS_ENOTEXIST, nullptr);
                }


            }

        _curleopt(CURLOPT_URL, url.c_str());

        busy = true;
        curl_multi_add_handle(gCurlMultiHandle, mCurl);
    }

{
    return new svc_http_request(cb, errb);
}
    void addHeader(const char* nameVal)
{
    mCustomHeaderscurl_slist hdr = curl_slist_append(NULL, nameVal);
    curl_easy_setopt(mCurl, CURLOPT_)
}

t_string_bounds services_http_url_get_host(const char* url)
{
    const char* p = url;
    for (; *p; p++)
    {
        char ch = *p;
        if (ch == '/')
            break;
    }
    if (!*p) //no slash till end of url - assume whole url is a host
        return {0, p-url};
    if ((p-url < 2) || !(*(p+1))) // 'x://' requires offset of at least 2 of first slash, and must have at least the next slash char
        return {-1,-1};

    if ((*(p-1) == ':') && (*(p+1) == '/')) //we are at the first slash of xxx://
    {
        p += 2; //go to the start of the host
        const char* end = url_find_host_end(p);
        if (!end)
            return {-1,-1};
        else
            return {p-url, end-url};
    }
    else
    {
        const char* end = url_find_host_end(url);
        if (!end)
            return {-1, -1};
        else
            return {0, end-url};
    }
}
const char* url_find_host_end(const char* p)
{
    bool hadSqBracket = (*p == '['); //an ipv6 address can be specified in an http url only in square brackets
    for (; *p; p++)
    {
        if (*p == '/')
            return p;
        else if (*p == ':')
        {
            if (!hadSqBracket) //not ipv6, must be port then
                return p;
        }
        else if (*p == ']')
        {
            if (hadSqBracket)
                return p; //[host] end, maybe ipv6
            else //invalid char
                return NULL;
        }
    }
    return p; //the terminating null
}

struct Transfer
{

};
