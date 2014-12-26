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
    ERRTYPE_HTTP = 0x3e9a4ffb
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
    ~Buffer() { if (mBuf) free(mBuf); }
    size_t bufSize() const {return mBufSize;}
    size_t dataSize() const {return mDataSize;}
    void ensureAppendSize(size_t size)
    {
        if (mBuf)
        {
            size += mDataSize;
            if (size > mBufSize)
                mBuf = (char*)realloc(mBuf, size);
            else
                return;
        }
        else
        {
            mBuf = (char*)malloc(size);
        }
        mBufSize = size;
    }
    const char* append(size_t writeSize)
    {
        ensureAppendSize(writeSize);
        char* appendPtr = mBuf+mDataSize;
        mDataSize+=writeSize;
        return appendPtr;
    }
    void clearData() { mDataSize = 0;}
    void shrinkToFitData(size_t maxReserve)
    {
        if (!mBuf)
            return;
        size_t maxSize = mDataSize+maxReserve;
        if (mDataSize > maxSize)
        {
            mBuf = (char*)realloc(mBuf, maxSize);
            mBufSize = maxSize;
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
    WriteAdapterBase(T& target): mSink(target){}
    T& sink() const {return mSink;}
};
template <class T>
class WriteAdapter;

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
    Client& client() {return mClient;}
//    virtual void append(const char* data, size_t len) = 0; //This method (and only this) is called by the libevent thread for performace reasons!!!
    virtual void onTransferComplete(int code, int type) = 0; //called by the GUI thread
    virtual ~ResponseBase(){}
};
template <class T>
class ResponseWithData: public ResponseBase
{
protected:
    std::shared_ptr<T> mSink;
    WriteAdapter<T> mWriter;
public:
    ResponseWithData(Client& client, std::shared_ptr<T> sink)
        :ResponseBase(client), mSink(sink), mWriter(*mSink){}
    std::shared_ptr<T>& data() {return mSink;}
    friend class Client;
};

template <class T, class CB>
class Response: public ResponseWithData<T>
{
    CB mCb;
public:
    Response(Client& client, CB&& aCb, std::shared_ptr<T> sink)
        :ResponseWithData<T>(client, sink ? sink : std::shared_ptr<T>(new T())),
        mCb(std::forward<CB>(aCb)) {}
    virtual void onTransferComplete(int code, int type)
    {
        mCb(code, type, ResponseWithData<T>::mSink);
    }
    friend class HttpClient;
};

#define _curleopt(opt, val) \
    do {                                                      \
    CURLcode ret = curl_easy_setopt(mCurl, opt, val);         \
    if (ret != CURLE_OK)                                      \
        throw std::runtime_error(std::string("curl_easy_setopt(")+ #opt +") at file "+ __FILE__+ ":"+std::to_string(__LINE__)); \
    } while(0)


class Client: public CurlConnection
{
protected:
    CURL* mCurl;
    int mMaxRetryWaitTime = 30;
    int mMaxRetryCount = 10;
    int status = 0;
    //the following two are updated directly by the libevent thread
    size_t mResponseLen = 0;
    size_t mCurrentRecvLen = 0;
    //===
    curl_slist* mCustomHeaders = nullptr;
//    std::unique_ptr<StreamSrcBase> mReader;
    std::unique_ptr<ResponseBase> mResponse;
public:
    Client()
    :mCurl(curl_easy_init())
    {
        if (!mCurl)
            throw std::runtime_error("Could not create a CURL easy handle");
        connOnComplete = onTransferComplete;

        _curleopt(CURLOPT_PRIVATE, this);
        _curleopt(CURLOPT_USERAGENT, services_http_useragent);
        _curleopt(CURLOPT_FOLLOWLOCATION, 1L);
        _curleopt(CURLOPT_AUTOREFERER, 1L);
        _curleopt(CURLOPT_MAXREDIRS, 5L);
        _curleopt(CURLOPT_CONNECTTIMEOUT, 30L);
        _curleopt(CURLOPT_TIMEOUT, 20L);
        _curleopt(CURLOPT_ACCEPT_ENCODING, ""); //enable compression
        _curleopt(CURLOPT_COOKIEFILE, "");
        _curleopt(CURLOPT_COOKIESESSION, 1L);
        _curleopt(CURLOPT_SSL_CTX_FUNCTION, &sslCtxFunction);
    }
protected:
    static void onTransferComplete(CurlConnection* conn, CURLcode code) //called by the CURL-libevent code
    {
        auto self = (Client*)conn;
        self->status = 0;
        if (self->mResponse)
            self->mResponse->onTransferComplete(code, ERRTYPE_HTTP);
    }
    static CURLcode sslCtxFunction(CURL* curl, void* sslctx, void*)
    {
        //TODO: Implement
        return CURLE_OK;
    }


    template <class R>
    void setupRecvAndStart(const std::string& url) //mResponse must be set before calling this
    {
        resolveUrlDomain(url,
         [this](int errcode, const std::string& url)
         {
            if (errcode)
                return mResponse->onTransferComplete(errcode, ERRTYPE_DNS);

            _curleopt(CURLOPT_WRITEDATA, mResponse.get());
            typedef size_t(*CURL_WRITEFUNC)(char *ptr, size_t size, size_t nmemb, void *userp);
            CURL_WRITEFUNC writefunc = [](char *ptr, size_t size, size_t nmemb, void *userp)
            {
                size_t len = size*nmemb;
                static_cast<R*>(userp)->mWriter.append((const char*)ptr, len);
                return len;
            };
            _curleopt(CURLOPT_WRITEFUNCTION, writefunc);
            _curleopt(CURLOPT_URL, url.c_str());
            status = 1;
            curl_multi_add_handle(gCurlMultiHandle, mCurl);
         });
    }
public:
    template <class T>
    promise::Promise<std::shared_ptr<T> >
    get(const std::string& url, std::shared_ptr<T> sink=nullptr)
    {
        promise::Promise<std::shared_ptr<T> > pms;
        get(url,
        [pms, this](int code, int type, std::shared_ptr<T> data) mutable
        {
            if ((type == ERRTYPE_HTTP) && (code == CURLE_OK))
                pms.resolve(data);
            else
                pms.reject(code, type);
        }, sink);
        return pms;
    }
    template <class T, class CB>
    void get(const std::string& url, CB&& cb, std::shared_ptr<T> sink=nullptr)
    {
        _curleopt(CURLOPT_HTTPGET, 1L);
        mResponse.reset(new Response<T, CB>(*this, std::forward<CB>(cb), sink));
        setupRecvAndStart<Response<T, CB> >(url);
    }
    template <class T, class CB>
    void post(const std::string& url, CB&& cb, const std::string& postData, std::shared_ptr<T> sink=nullptr)
    {
        _curleopt(CURLOPT_POSTFIELDS, postData.c_str());
        _curleopt(CURLOPT_POSTFIELDSIZE, (long)postData.size());
        post(url, std::forward<CB>(cb), sink);
    }
    template <class T>
    promise::Promise<std::shared_ptr<T> >
    post(const std::string& url, const char* postData, std::shared_ptr<T> sink=nullptr)
    {
        assert(postData);
        _curleopt(CURLOPT_POSTFIELDS, postData);
        return get(url, postData, sink);
    }
/*
    template <class R, class WT>
    void postStream(const std::string& url) //mResponse and mReader must be set before calling this
    {
        _curleopt(CURLOPT_READFUNCTION,
        [](char *buffer, size_t size, size_t nitems, void* userp)
        {
            auto src = (StreamSrc*)userp;
            size_t requested = size*nitems;
            return src->read(requested, buffer);
        });
        _curleopt(CURLOPT_READDATA, this.mReader.get());
        _curleopt(CURLOPT_HTTPPOST, 1L);
    }
public:
*/
protected:
    template <class CB>
    void resolveUrlDomain(const std::string& url, const CB& cb)
    {
        auto bounds = services_http_url_get_host(url.c_str());
        auto type = services_dns_host_type(url.c_str()+bounds.start, url.c_str()+bounds.end);
        printf("host = %s\n", url.substr(bounds.start, bounds.end-bounds.start).c_str());
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
            std::string domain(url.c_str()+bounds.start, bounds.end-bounds.start);
            dnsLookup(domain.c_str(), services_http_use_ipv6?SVCF_DNS_IPV6:SVCF_DNS_IPV4,
            [this, cb, bounds, url](int errCode, std::shared_ptr<AddrInfo>&& addrs)
            {
                if (errCode)
                {
                    return cb(errCode, url);
                }
                size_t hostlen = bounds.end-bounds.start;
                std::string newUrl = url;
                if (!services_http_use_ipv6)
                {
                   if (addrs->ip4addrs().empty())
                       return cb(SVCDNS_ENOTEXIST, nullptr); //TODO: Find proper error code
                   newUrl.replace((size_t)bounds.start, hostlen, addrs->ip4addrs()[0].toString());
                }
                else
                {
                    if (addrs->ip6addrs().empty())
                        return cb(SVCDNS_ENOTEXIST, nullptr);
                    newUrl.replace((size_t)bounds.start, hostlen, addrs->ip6addrs()[0].toString());
                }
                return cb(SVCDNS_ESUCCESS, newUrl);
            });
        }
        else
        {
            KR_LOG_ERROR("%s: unknown type: %d returned from services_dns_host_type()", __FUNCTION__ , type);
        }
    }
};
}
}
 //   void addHeader(const char* nameVal)
//{
//    mCustomHeaderscurl_slist hdr = curl_slist_append(NULL, nameVal);
//    curl_easy_setopt(mCurl, CURLOPT_)
//}
