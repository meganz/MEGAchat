#include "cservices.h"
#include "services-dns.hpp"
#include <curl/curl.h>
#include <event2/event.h>

#define always_assert(cond) \
    if (!(cond)) SVC_LOG_ERROR("HTTP: Assertion failed: '%s' at file %s, line %d", #cond, __FILE__, __LINE__)

namespace mega
{
enum
{
    ERRTYPE_HTTP = 0x3e9a4ffb;
};

namespace http //deserves its own namespace
{
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

//Polymorphic decoupling of http response and completion callback types
class ResponseBase
{
protected:
    Client& mClient;
public:
    ResponseBase(Client& client): mClient(client){}
    Client& client() const {return mClient;}
//    virtual void append(const char* data, size_t len) = 0; //This method (and only this) is called by the libevent thread for performace reasons!!!
    virtual void onTransferComplete(int code, int type) = 0; //called by the GUI thread
    virtual ~ResponseBase(){}
};

template <class T, class CB>
class Response: public ResponseBase
{
    std::shared_ptr<T> mSink;
    WriteAdapter<T> mWriter;
    CB mCb;
public:
    Response(Client& client, CB&& aCb, std::shared_ptr<T> sink)
        :ResponseBase(client), mSink(sink), mWriter(sink), mCb(std::forward<CB>(aCb))
    {}
    virtual void onTransferComplete(int code, int type)
    {
        mCb(code, type);
    }
    friend class HttpClient;
};

template <class T, class CB>
class ResponseWithOwnData: public Response<T, CB>
{
public:
    std::shared_ptr<T> data;
    ResponseWithOwnData(Client &client, CB&& aCb, size_t initialSize)
        :Response<T, CB>(client, std::forward<CB>(aCb), *(new T), initialSize)
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
    friend class HttpClient;
};

#define _curleopt(opt, val) \
    do {                                                      \
    CURLcode ret = curl_easy_setopt(mCurl, opt, val);         \
    if (ret != CURLE_OK)                                      \
        throw std::runtime_error(std::string("curl_easy_setopt(")+ #opt +") at file "+ __FILE__+ ":"+std::to_string(__LINE__)); \
    } while(0)


class HttpClient: public CurlConnection
{
protected:
    CURL* mCurl;
    int mMaxRetryWaitTime = 30;
    int mMaxRetryCount = 10;
    bool busy = false;
    //the following two are updated directly by the libevent thread
    size_t mResponseLen = 0;
    size_t mCurrentRecvLen = 0;
    //===
    curl_slist* mCustomHeaders = nullptr;
    void* mReader = nullptr;
    std::unique_ptr<ResponseBase> mResponse;
    HttpClient()
    :curl(curl_easy_init())
    {
        if (!curl)
            throw runtime_error("Could not create a CURL easy handle");
        it ret = curl_multi_add_handle(gCurlMultiHandle, curl);
        if (ret != CURLE_OK)
            throw runtime_error("Could not add CURL easy handle to multi handle");
        conn.onComplete = onTransferComplete;

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
    static void onTransferComplete(CurlConnection* conn, CURLCode code) //called by the CURL-libevent code
    {
        auto self = (HttpClient*)conn;
        self->busy = false;
        if (self->mResponse)
            self->mResponse->onTransferComplete(code, ERRTYPE_HTTP);
    }

    template <class R>
    void setupRecvAndStart(const std::string& url) //mResponse must be set before calling this
    {
        resolveUrlDomain(url,
        [this, response](int errcode, const std::string& url)
        {
            if (errcode)
                return response->onTransferComplete(errcode, ERRTYPE_DNS);

            _curleopt(CURLOPT_WRITEDATA, this->mResponse.get());
            auto writefunc = [](char *ptr, size_t size, size_t nmemb, void *userp)
            {
                size_t len = size*nmemb;
                static_cast<R*>(userp)->mWriter.append((const char*)ptr, len);
                return len;
            };
            _curleopt(CURLOPT_WRITEFUNCTION, writefunc);
            _curleopt(CURLOPT_URL, url.c_str());
            busy = true;
            curl_multi_add_handle(gCurlMultiHandle, mCurl);
        });
    }
    template <class T>
    promise::Promise<ResponseWithOwnData<T>& >void get(const std::string& url)
    {
        promise::Promise<ResponseWithOwnData<T> >pms;
        get(url,
        [pms, this](int code, int type)
        {
            if ((type == ERRTYPE_HTTP) && (code == CURLE_OK))
                pms.resolve(*mResponse);
            else
                pms.reject(code, type);
        });
    }
    template <class T, class CB>
    void get(const std::string& url, CB&& cb)
    {
        _curleopt(mCurl, CURLOPT_GET, 1L);
        mResponse.reset(new ResponseWithOwnData<T>(this, std::forward<CB>(cb)));
        setupRecvAndStart<ResponseWithOwnData<T> >(url);
    }
    template <class R, class WT>
    void post(const std::string& url)
    {

    }

    template <class CB>
    void resolveUrlDomain(const std::string& url, const CB& cb)
    {
        auto bounds = services_http_url_get_host(url.c_str());
        auto type = services_dns_host_type(bounds.start, bounds.end);
        if (type & SVC_DNS_HOST_IS_IP)
        {
            return cb(SVCDNS_ESUCCESS, url);
        }
        else if (type == SVC_DNS_HOST_INVALID)
        {
            return cb(SVCDNS_EFORMAT, url);
        }
        else if (type == SVC_DNS_HOST_DOMAIN)
        {
            string domain(bounds.start, hostlen);
            dnsLookup(domain.c_str(), gIpMode,
            [this, cb, bounds](int errCode, std::shared_ptr<AddrInfo>& addrs)
            {
                if (errCode)
                {
                    return cb(errCode, nullptr);
                }
                size_t hostlen = bounds.end-bounds.start;
                if (!gUseIpV6)
                {
                   if (addrs->ip4addrs().empty())
                       return cb(SVCDNS_ENOTEXIST, nullptr); //TODO: Find proper error code
                   std::string newUrl = url;
                   newUrl.replace(bounds.start, hostlen, addrs->ip4addrs[0]);
                }
                else
                {
                    if (addrs->ip6addrs().empty())
                        return cb(SVCDNS_ENOTEXIST, nullptr);
                    std::string newUrl = url;
                    newUrl.replace(bounds.start, hostlen, addrs->ip6addrs[0]);
                }
                return cb(SVCDNS_ESUCCESS, newUrl);
            });
        }
        else
        {
            KR_LOG_ERROR(__FUNCTION__ ": unknown type: %d returned from services_dns_host_type()", type);
        }
    }
    void addHeader(const char* nameVal)
{
    mCustomHeaderscurl_slist hdr = curl_slist_append(NULL, nameVal);
    curl_easy_setopt(mCurl, CURLOPT_)
}
