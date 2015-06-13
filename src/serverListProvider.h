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
#define SRVJSON_CHECK_GET_PROP(varname, name, type)                                             \
    auto varname = json.FindMember(#name);                                                \
    if (varname == json.MemberEnd())                                                        \
        throw std::runtime_error("No '" #name "' field in JSON server item");         \
    if (!varname->value.Is##type())                                                              \
        throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'")

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
                    return promise::Error("No servers", 0x3e9a9e1b, 1);
                else
                    return B::at(B::mNextAssignIdx++);
            });
        }
        else
        {
            return B::at(B::mNextAssignIdx++);
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
    StaticProvider(const char* servers);
    promise::Promise<void> fetchServers() { this->mNextAssignIdx = 0; }
};

template <class S=HostPortServerInfo>
class GelbProvider: public ServerList<S>
{
protected:
    std::string mGelbHost;
    std::string mService;
    size_t mReqTimeout;
    size_t mMaxAttempts;
    int64_t mMaxReuseOldServersAge;
    int64_t mLastUpdateTs = 0;
    std::shared_ptr<mega::http::Client> mClient; //can only get away with shared_ptr and incomplete Client class
    void parseServerList(const std::string& json);
public:
    typedef S Server;
    promise::Promise<int> fetchServers();
    GelbProvider(const char* gelbHost, const char* service, int64_t maxReuseOldServersAge=0,
        size_t maxAttempts=3, size_t reqTimeout=4000)
    :mGelbHost(gelbHost), mService(service), mReqTimeout(reqTimeout), mMaxAttempts(maxAttempts),
      mMaxReuseOldServersAge(maxReuseOldServersAge)
    {}
};

template <class S>
promise::Promise<int> GelbProvider<S>::fetchServers()
{
//    auto client = new mega::http::Client;

    std::shared_ptr<mega::http::Client> client(new mega::http::Client);
    return ::mega::retry([this, client](int no) -> promise::Promise<int>
    {
        return client->pget<std::string>(mGelbHost+"/?service="+mService)
        .then([this](std::shared_ptr<mega::http::Response<std::string> > response)
            -> promise::Promise<int>
        {
            if (response->httpCode() != 200)
            {
                return promise::Error("Non-200 http response from GeLB server: "
                                      +*response->data(), 0x3e9a9e1b, 1);
            }
            parseServerList(*(response->data()));
            this->mNextAssignIdx = 0; //notify about updated servers only if parse didn't throw
            this->mLastUpdateTs = timestampMs();
            return 0;
        });
    }, [this, client](){client->abort();}, mReqTimeout, mMaxAttempts, 0)
    .fail([this](const promise::Error& err) -> promise::Promise<int>
    {
        if (this->mLastUpdateTs && ((timestampMs() - mLastUpdateTs) < mMaxReuseOldServersAge))
        {
            this->mNextAssignIdx = 0;
            KR_LOG_WARNING("Gelb client: error getting new servers, reusing not too old servers that we have from gelb");
            return 0;
        }
        else
            return err;
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
void GelbProvider<S>::parseServerList(const std::string& json)
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
    if (!arr->value.IsArray())
        throw std::runtime_error("JSON received from GeLB is not an array");
    for (auto it=arr->value.Begin(); it!=arr->value.End(); ++it)
    {
        if (!it->IsObject())
            throw std::runtime_error("Server info entry is not an object");
        this->emplace_back(new S(*it));
    }
}
}

#endif
