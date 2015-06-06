#include "cservices.h"
#include "services-dns.hpp"
#include <curl/curl.h>
#include <event2/event.h>

#define always_assert(cond) \
    if (!(cond)) SVC_LOG_ERROR("HTTP: Assertion failed: '%s' at file %s, line %d", #cond, __FILE__, __LINE__)
#define KRHTTP_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_http, fmtString, ##__VA_ARGS__)
#define KRHTTP_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_http, fmtString, ##__VA_ARGS__)

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
    virtual void onTransferComplete(int code, int type);
    friend class HttpClient;
};
const char* strNotNull(const char* str)
{
    if (!str)
        return "(null)";
    else
        return str;
}

#define _curleopt(opt, val) \
    do {                                                      \
    CURLcode ret = curl_easy_setopt(mCurl, opt, val);         \
    if (ret != CURLE_OK)                                      \
        throw std::runtime_error(std::string("curl_easy_setopt(")+ #opt +") failed with code "+ \
        std::to_string(ret)+" ["+strNotNull(curl_easy_strerror(ret))+"] at file "+ __FILE__+ ":"+std::to_string(__LINE__)); \
    } while(0)


class Client: public CurlConnection
{
protected:
    CURL* mCurl;
    bool mBusy = false;
    std::string mUrl;
    curl_slist* mCustomHeaders = nullptr;
//    std::unique_ptr<StreamSrcBase> mReader;
    std::unique_ptr<ResponseBase> mResponse;
public:
    const std::string& url() const { return mUrl; }
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
        _curleopt(CURLOPT_SSL_VERIFYPEER, 0L);
        _curleopt(CURLOPT_SSL_VERIFYHOST, 0L);

    }
protected:
    static void onTransferComplete(CurlConnection* conn, CURLcode code) //called by the CURL-libevent code
    {
        auto self = (Client*)conn;
        if (code == CURLE_OK)
         {
            long httpCode;
            curl_easy_getinfo(self->mCurl, CURLINFO_RESPONSE_CODE, &httpCode);
            KRHTTP_LOG_DEBUG("Completed request to '%s' with status %d", self->mUrl.c_str(), httpCode);
            if (self->mResponse)
                self->mResponse->onTransferComplete(httpCode, 0);
        }
        else
        {
            KRHTTP_LOG_DEBUG("Request to '%s' failed with error code: %d [%s]",
                self->mUrl.c_str(), code, curl_easy_strerror((CURLcode)code));
            if (self->mResponse)
                self->mResponse->onTransferComplete(code, ERRTYPE_HTTP);

        }

        self->mBusy = false;
    }
    static CURLcode sslCtxFunction(CURL* curl, void* sslctx, void*)
    {
        //TODO: Implement
        return CURLE_OK;
    }


    template <class R>
    void setupRecvAndStart(const std::string& aUrl) //mResponse must be set before calling this
    {
        mBusy = true;
        mUrl = aUrl;
        resolveUrlDomain(aUrl,
         [this](int errcode, const std::string& url)
         {
            if (errcode)
            {
                KRHTTP_LOG_ERROR("DNS error %d on request to '%s'", errcode, mUrl.c_str());
                mResponse->onTransferComplete(errcode, ERRTYPE_DNS);
                mBusy = false;
                return;
            }
            typedef size_t(*WriteFunc)(char *ptr, size_t size, size_t nmemb, void *userp);
            WriteFunc writefunc = [](char *ptr, size_t size, size_t nmemb, void *userp)
            {
                size_t len = size*nmemb;
                static_cast<R*>(userp)->mWriter.append((const char*)ptr, len);
                return len;
            };
            _curleopt(CURLOPT_WRITEFUNCTION, writefunc);
            _curleopt(CURLOPT_WRITEDATA, mResponse.get());
            _curleopt(CURLOPT_URL, url.c_str());
            KRHTTP_LOG_DEBUG("Starting request '%s'...", mUrl.c_str());
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
            if (type == 0)
            {
                pms.resolve(data);
            }
            else
            {
                if (code == ERRTYPE_HTTP)
                    pms.reject(promise::Error(curl_easy_strerror((CURLcode)code), code, type));
                else
                    pms.reject(code, type);
            }
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
    void post(const std::string& url, CB&& cb, const char* postData, size_t postDataSize, std::shared_ptr<T> sink=nullptr)
    {
        assert(postData);
        _curleopt(CURLOPT_HTTPPOST, 1L);
        _curleopt(CURLOPT_POSTFIELDS, postData);
        _curleopt(CURLOPT_POSTFIELDSIZE, (long)postDataSize);
        mResponse.reset(new Response<T, CB>(*this, std::forward<CB>(cb), sink));
        setupRecvAndStart<Response<T, CB> >(url);
    }
    template <class T>
    promise::Promise<std::shared_ptr<T> >
    post(const std::string& url, const char* postData, size_t postDataSize, std::shared_ptr<T> sink=nullptr)
    {
        promise::Promise<std::shared_ptr<T> > pms;
        post(url,
        [pms, this](int code, int type, std::shared_ptr<T> data) mutable
        {
            if (type == 0)
            {
                pms.resolve(data);
            }
            else
            {
                if (code == ERRTYPE_HTTP)
                    pms.reject(promise::Error(curl_easy_strerror((CURLcode)code), code, type));
                else
                    pms.reject(code, type);
            }
        }, postData, postDataSize, sink);
        return pms;
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
        if (type & SVC_DNS_HOST_IS_IP)
        {
            KRHTTP_LOG_DEBUG("resolveUrlDomain: Host is already an IP");
            return cb(SVCDNS_ESUCCESS, url);
        }
        else if (type == SVC_DNS_HOST_INVALID)
        {
            KRHTTP_LOG_DEBUG("resolveUrlDomain: '%s' is not a vaild host name", url.substr(bounds.start, bounds.end-bounds.start).c_str());
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
                if (!services_http_use_ipv6 || addrs->ip6addrs().empty())
                {
                    if (addrs->ip4addrs().empty())
                        return cb(SVCDNS_ENOTEXIST, nullptr); //TODO: Find proper error code

                    KRHTTP_LOG_DEBUG("Resolved '%.*s' -> ipv4 '%s'",
                          bounds.end-bounds.start, url.c_str()+bounds.start, addrs->ip4addrs()[0].toString());
                   newUrl.replace((size_t)bounds.start, hostlen, addrs->ip4addrs()[0].toString());
                }
                else
                {
                    KRHTTP_LOG_DEBUG("Resolved '.*%s' -> ipv6 '%s'",
                        bounds.end-bounds.start, url.c_str()+bounds.start, addrs->ip6addrs()[0].toString());
                    newUrl.replace((size_t)bounds.start, hostlen, addrs->ip6addrs()[0].toString());
                }
                return cb(SVCDNS_ESUCCESS, newUrl);
            });
        }
        else
        {
            KRHTTP_LOG_ERROR("%s: unknown type: %d returned from services_dns_host_type()", __FUNCTION__ , type);
        }
    }
};

template <class T, class CB>
void Response<T,CB>::onTransferComplete(int code, int type)
{
    mCb(code, type, ResponseWithData<T>::mSink);
}

}
}
 //   void addHeader(const char* nameVal)
//{
//    mCustomHeaderscurl_slist hdr = curl_slist_append(NULL, nameVal);
//    curl_easy_setopt(mCurl, CURLOPT_)
//}
