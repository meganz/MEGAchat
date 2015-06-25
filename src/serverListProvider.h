#ifndef _SERVERLIST_PROVIDER_H
#define _SERVERLIST_PROVIDER_H
#include <string>
#include <vector>
#include <promise.h>
#include <rapidjson/document.h>
#include <base/services-http.hpp>
#include "retryHandler.h"

namespace mega { namespace http { class Client; } }
namespace karere
{
#define SRVJSON_CHECK_GET_PROP(varname, name, type)                                              \
    auto varname = json.FindMember(#name);                                                       \
    if (varname == json.MemberEnd())                                                             \
        throw std::runtime_error("No '" #name "' field in JSON server item");                    \
    if (!varname->value.Is##type())                                                              \
        throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'")

template <class S>
void parseServerList(const rapidjson::Value& arr, std::vector<std::shared_ptr<S> >& servers);
static inline const char* strOrEmpty(const char* str);
struct HostPortServerInfo
{
    std::string host;
    unsigned short port;
    HostPortServerInfo(const rapidjson::Value& json)
    {
        SRVJSON_CHECK_GET_PROP(vHost, host, String);
        SRVJSON_CHECK_GET_PROP(vPort, port, Int);
        int aPort = vPort->value.GetInt();
        if (aPort < 0 || aPort > 65535)
            throw std::runtime_error("HostPortServerInfo: Port "+std::to_string(aPort)+" is out of range");
        port = (unsigned short)aPort;
        host = vHost->value.GetString();
    }
};

template <class S>
class ServerList: public std::vector<std::shared_ptr<S> >
{
protected:
    size_t mNextAssignIdx = 0;
    bool needsUpdate() const { return ((mNextAssignIdx >= this->size()) || this->empty()); }

};

template <class B>
class SingleServerProvider: public B
{
public:
    using B::B;
    promise::Promise<std::shared_ptr<typename B::Server> > getServer()
    {
        if (B::needsUpdate())
        {
            return B::fetchServers()
            .then([this](int) -> promise::Promise<std::shared_ptr<typename B::Server> >
            {
                if (B::needsUpdate())
                {
                    return promise::Error("No servers", 0x3e9a9e1b, 1);
                }
                else
                {
                    printf("returning server\n");
                    return B::at(B::mNextAssignIdx++);
                }
            });
        }
        else
        {
            promise::Promise<std::shared_ptr<typename B::Server> > pms;
            mega::setTimeout([pms, this]() mutable
            {
                pms.resolve(B::at(B::mNextAssignIdx++));
            }, 0);
            return pms;

//            return B::at(B::mNextAssignIdx++);
        }
    }
};
template <class B>
class ServerListProvider: public B
{
public:
    using B::B;
    promise::Promise<std::shared_ptr<typename B::Server> > getServers()
    {
        if (B::needsUpdate())
        {
            return B::fetchServers()
            .then([this]()
            {
                if (B::needsUpdate())
                    return nullptr;
                B::mNextAssignIdx+=B::size();
                return new ServerList<typename B::Server>(*this);
            });
        }
        else
        {
            B::mNextAssignIdx+=B::size();
            return std::shared_ptr<ServerList<typename B::Server> >
                    (new ServerList<typename B::Server>(*this));
        }
    }
};

template <class S=HostPortServerInfo>
class StaticProvider: public ServerList<S>
{
protected:
public:
    typedef S Server;
    StaticProvider(const char* serversJson)
    {
        rapidjson::Document doc;
        doc.Parse<0>(serversJson);
        if (doc.HasParseError())
        {
            throw std::runtime_error(std::string("Error parsing json: ")+strOrEmpty(doc.GetParseError())
                                     +" at position "+std::to_string(doc.GetErrorOffset()));
        }
        parseServerList(doc, *this);
    }
    promise::Promise<int> fetchServers() { this->mNextAssignIdx = 0; return 0;}
};

template <class S=HostPortServerInfo>
class GelbProvider: public ServerList<S>
{
protected:
    std::string mGelbHost;
    std::string mService;
    int64_t mMaxReuseOldServersAge;
    int64_t mLastUpdateTs = 0;
    std::unique_ptr<mega::http::Client> mClient;
    std::unique_ptr<mega::rh::IRetryController> mRetryController;
    void parseServersJson(const std::string& json);
public:
    typedef S Server;
    promise::Promise<int> fetchServers();
    GelbProvider(const char* gelbHost, const char* service, int reqCount=2, unsigned reqTimeout=4000,
        int64_t maxReuseOldServersAge=0);
    void abort()
    {
        if (!mClient)
            return;
        mRetryController->abort();
        assert(!mClient);
    }
};
template <class S=HostPortServerInfo>
class FallbackSingleServerProvider
{
protected:
    SingleServerProvider<GelbProvider<S> > mGelbProvider;
    SingleServerProvider<StaticProvider<S> > mStaticProvider;
    int mGelbReqRetryCount;
    unsigned mGelbReqTimeout;
public:
    FallbackSingleServerProvider(const char* gelbHost, const char* service, const char* staticServers,
        int64_t gelbMaxReuseAge=0, int gelbRetryCount=2, unsigned gelbReqTimeout=4000)
        :mGelbProvider(gelbHost, service, gelbRetryCount, gelbReqTimeout, gelbMaxReuseAge),
        mStaticProvider(staticServers)
    {}
    promise::Promise<std::shared_ptr<S> > getServer()
    {
        return mGelbProvider.getServer()
        .fail([this](const promise::Error& err)
        {
            KR_LOG_ERROR("Gelb request failed, falling back to static server list");
            return mStaticProvider.getServer();
        });
    }
    void abort()
    {
        mGelbProvider.abort();
    }
};

template <class S>
GelbProvider<S>::GelbProvider(const char* gelbHost, const char* service,
    int reqCount, unsigned reqTimeout, int64_t maxReuseOldServersAge)
    :mGelbHost(gelbHost), mService(service), mMaxReuseOldServersAge(maxReuseOldServersAge)
{
    mRetryController.reset(mega::createRetryController(
    [this](int no)
    {
        assert(mClient && !mClient->busy());
        return mClient->pget<std::string>(mGelbHost+"/?service="+mService)
        .then([this](std::shared_ptr<mega::http::Response<std::string> > response)
            -> promise::Promise<int>
        {
            mClient.reset();
            if (response->httpCode() != 200)
            {
                return promise::Error("Non-200 http response from GeLB server: "
                                      +*response->data(), 0x3e9a9e1b, 1);
            }
            parseServersJson(*(response->data()));
            this->mNextAssignIdx = 0; //notify about updated servers only if parse didn't throw
            this->mLastUpdateTs = timestampMs();
            return 0;
        });
    },
    [this]()
    {
        if (mClient) //if this was the last attempt, the output promise will fail and reset() to nullptr
            mClient->abort();
    }, reqTimeout, reqCount, 1, 1));
}

template <class S>
promise::Promise<int> GelbProvider<S>::fetchServers()
{
    if (mClient)
        throw std::runtime_error("GeLB provider: a request is already in progress");
    mClient.reset(new mega::http::Client);
    mRetryController->reset();
    return static_cast<promise::Promise<int>& > (mRetryController->start())
    .fail([this](const promise::Error& err) -> promise::Promise<int>
    {
        mClient.reset();
        if (!this->mLastUpdateTs || ((timestampMs() - mLastUpdateTs) > mMaxReuseOldServersAge))
        {
            return err;
        }
        this->mNextAssignIdx = 0;
        KR_LOG_WARNING("Gelb client: error getting new servers, reusing not too old servers that we have from gelb");
        return 0;
    });
}

static inline const char* strOrEmpty(const char* str)
{
    if (!str)
        return "";
    else
        return str;
}

template <class S>
void GelbProvider<S>::parseServersJson(const std::string& json)
{
    rapidjson::Document doc;
    doc.Parse<0>(json.c_str());
    if (doc.HasParseError())
    {
        throw std::runtime_error(std::string("Error parsing json: ")+strOrEmpty(doc.GetParseError())
                                 +" at position "+std::to_string(doc.GetErrorOffset()));
    }
    auto ok = doc.FindMember("ok");
    if ((ok == doc.MemberEnd()) || (!ok->value.IsInt()) || (!ok->value.GetInt()))
            throw std::runtime_error("No ok:1 flag in JSON returned from GeLB");
    auto arr = doc.FindMember(mService.c_str());
    if (arr == doc.MemberEnd())
        throw std::runtime_error("JSON receoved does not have a '"+mService+"' member");
    parseServerList(arr->value, *this);
}

template <class S>
void parseServerList(const rapidjson::Value& arr, std::vector<std::shared_ptr<S> >& servers)
{
    if (!arr.IsArray())
        throw std::runtime_error("JSON received from GeLB is not an array");
    std::vector<std::shared_ptr<S> > parsed;
    for (auto it=arr.Begin(); it!=arr.End(); ++it)
    {
        if (!it->IsObject())
            throw std::runtime_error("Server info entry is not an object");
        parsed.emplace_back(new S(*it));
    }
    servers.swap(parsed);
}
}

#endif
