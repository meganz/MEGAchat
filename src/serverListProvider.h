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
    auto it_##varname = json.FindMember(#name);                                                 \
    if (!it_##varname)                                                                          \
        throw std::runtime_error("No '" #name "' field in JSON server item");                    \
    if (!it_##varname->value.Is##type())                                                        \
        throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'");            \
    varname = it_##varname->value.Get##type();

#define SRVJSON_GET_OPTIONAL_PROP(varname, name, type)                                           \
    auto it_##name = json.FindMember(#name);                                                     \
    if (it_##name)                                                                               \
    {                                                                                            \
        if (!it_##name->value.Is##type())                                                        \
            throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'");       \
        varname = it_##name->value.Get##type();                                                     \
    }

template <class S>
void parseServerList(const rapidjson::Value& arr, std::vector<std::shared_ptr<S> >& servers);
static inline const char* strOrEmpty(const char* str);

struct HostPortServerInfo
{
    std::string host;
    unsigned short port;
    HostPortServerInfo(const rapidjson::Value& json)
    {
        SRVJSON_CHECK_GET_PROP(host, host, String);
        int vPort;
        SRVJSON_CHECK_GET_PROP(vPort, port, Int);
        if (vPort < 0 || vPort > 65535)
            throw std::runtime_error("HostPortServerInfo: Port "+std::to_string(vPort)+" is out of range");
        port = vPort;
    }
};

struct TurnServerInfo
{
    std::string url;
    std::string user;
    std::string pass;
    TurnServerInfo(const rapidjson::Value& json)
    {
        SRVJSON_CHECK_GET_PROP(url, host, String);
        if (url.substr(0, 5) != "turn:")
        {
            KR_LOG_WARNING("TURN server url missing 'turn:' prefix, adding it");
            url = "turn:"+url;
//TODO: Remove once the gelb JSON is ok
            int port = 0;
            SRVJSON_GET_OPTIONAL_PROP(port, port, Int);
            if (port > 0)
            {
                url+=+":";
                url+=std::to_string(port);
            }
        }
        SRVJSON_GET_OPTIONAL_PROP(user, user, String);
        SRVJSON_GET_OPTIONAL_PROP(pass, pass, String);
    }
};

/** A list of server info structures, defined by the class S */
template <class S>
using ServerList = std::vector<std::shared_ptr<S> >;

/** An abstract class that provides a list of servers (i.e. more than one). A single server info is
 * contained in the class S
 */
template <class S>
class ListProvider: public std::shared_ptr<ServerList<S> >
{
public: //must be protected, but because of a gcc bug, protected/private members cant be accessed from within a lambda
    typedef std::shared_ptr<ServerList<S> > Base;
    ListProvider():Base(new ServerList<S>()){}
    size_t mNextAssignIdx = 0;
    bool needsUpdate() const { return ((mNextAssignIdx >= (*this)->size()) || (*this)->empty()); }
};

/** The frontend server provider API class - can provide a single or multile servers,
 * relying on a server data provider implemented by the class B
 */
template <class B>
class ServerProvider
{
protected:
    std::unique_ptr<B> mBase;
public:
    ServerProvider(B* base): mBase(base){}
    promise::Promise<std::shared_ptr<typename B::Server> > getServer(unsigned timeout=0)
    {
        if (mBase->needsUpdate())
        {
            return mBase->fetchServers(timeout)
            .then([this]() -> promise::Promise<std::shared_ptr<typename B::Server> >
            {
                if (mBase->needsUpdate())
                    return promise::Error("No servers", 0x3e9a9e1b, 1);
                else
                    return (*mBase)->at(mBase->mNextAssignIdx++);
            });
        }
        else
        {
            return (*mBase)->at(mBase->mNextAssignIdx++);
        }
    }
    promise::Promise<std::shared_ptr<ServerList<typename B::Server> > > getServers(unsigned timeout=0)
    {
        if (mBase->needsUpdate())
        {
            return mBase->fetchServers(timeout)
            .then([this]() -> promise::Promise<std::shared_ptr<ServerList<typename B::Server> > >
            {
                if (mBase->needsUpdate())
                    return promise::Error("No servers", 0x3e9a9e1b, 1);
                mBase->mNextAssignIdx += (*(this->mBase))->size();
                return *(this->mBase);
            });
        }
        else
        {
            mBase->mNextAssignIdx += (*(this->mBase))->size();
            return *(this->mBase);
        }
    }
};

/** An implementation of a server data provider that gets the servers from a local, static list,
 *  possibly hardcoded
 */
template <class S>
class StaticProvider: public ListProvider<S>
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
        parseServerList(doc, **this);
    }
    promise::Promise<void> fetchServers(unsigned timeout=0) { this->mNextAssignIdx = 0; return promise::_Void();}
};

/** An implementation of a server data provider that gets the servers from the GeLB server */
template <class S>
class GelbProvider: public ListProvider<S>
{
protected:
    std::string mGelbHost;
    std::string mService;
    int64_t mMaxReuseOldServersAge;
    int64_t mLastUpdateTs = 0;
    std::unique_ptr<mega::http::Client> mClient;
    std::unique_ptr<mega::rh::IRetryController> mRetryController;
    promise::Promise<void> mOutputPromise;
    void parseServersJson(const std::string& json);
    promise::Promise<void> exec(int no);
    void giveup();
public:
    typedef S Server;
    promise::Promise<void> fetchServers(unsigned timeout=0);
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
/** Another public API server provider that sits on top of one GeLB and one static providers.
 *  It tries first to get servers via GeLB and if it doesn't sucees, falls back to the static provider
 */
template <class S>
class FallbackServerProvider
{
protected:
    ServerProvider<GelbProvider<S> > mGelbProvider;
    ServerProvider<StaticProvider<S> > mStaticProvider;
    int mGelbReqRetryCount;
    unsigned mGelbReqTimeout;
public:
    FallbackServerProvider(const char* gelbHost, const char* service, const char* staticServers,
        int64_t gelbMaxReuseAge=0, int gelbRetryCount=2, unsigned gelbReqTimeout=4000)
        :mGelbProvider(new GelbProvider<S>(gelbHost, service, gelbRetryCount, gelbReqTimeout, gelbMaxReuseAge)),
        mStaticProvider(new StaticProvider<S>(staticServers))
    {}
    promise::Promise<std::shared_ptr<S> > getServer(unsigned timeout=0)
    {
        return mGelbProvider.getServer(timeout)
        .fail([this](const promise::Error& err)
        {
            KR_LOG_ERROR("Gelb request failed with error '%s', falling back to static server list", err.what());
            return mStaticProvider.getServer();
        });
    }
    promise::Promise<std::shared_ptr<ServerList<S> > > getServers(unsigned timeout = 0)
    {
        return mGelbProvider.getServers(timeout)
        .fail([this](const promise::Error& err)
        {
            KR_LOG_ERROR("Gelb request failed with error '%s', falling back to static server list", err.what());
            return mStaticProvider.getServers();
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
        [this](int no) { return exec(no); },
        [this]() { giveup(); },
        reqTimeout, reqCount, 1, 1
    ));
}
template <class S>
promise::Promise<void> GelbProvider<S>::exec(int no)
{
    mClient.reset(new mega::http::Client);
    return mClient->pget<std::string>(mGelbHost+"/?service="+mService)
    .then([this](std::shared_ptr<mega::http::Response<std::string> > response)
        -> promise::Promise<void>
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
        return promise::_Void();
    });
}
template <class S>
void GelbProvider<S>::giveup()
{
    if (mClient) //if this was the last attempt, the output promise will fail and reset() to nullptr
        mClient->abort();
}

template <class S>
promise::Promise<void> GelbProvider<S>::fetchServers(unsigned timeout)
{
    if (mClient)
    {
        return mOutputPromise;
    }

    mRetryController->reset();
    promise::Promise<void> pms;
    if (timeout)
    {
        pms = exec(0);
        ::mega::setTimeout([this]() { giveup(); }, timeout);
    }
    else
    {
        pms = static_cast<promise::Promise<void>& > (mRetryController->start());
    }

    mOutputPromise = pms
    .fail([this](const promise::Error& err) -> promise::Promise<void>
    {
        mClient.reset();
        if (!this->mLastUpdateTs || ((timestampMs() - mLastUpdateTs)/1000 > mMaxReuseOldServersAge))
        {
            return err;
        }
        this->mNextAssignIdx = 0;
        KR_LOG_WARNING("Gelb client: error getting new servers, reusing not too old servers that we have from gelb");
        return promise::_Void();
    });
    return mOutputPromise;
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
    auto arr = doc.FindMember(mService.c_str());
    if (arr == doc.MemberEnd())
        throw std::runtime_error("JSON receoved does not have a '"+mService+"' member");
    parseServerList(arr->value, **this);
}

template <class S>
void parseServerList(const rapidjson::Value& arr, std::vector<std::shared_ptr<S> >& servers)
{
    if (!arr.IsArray())
        throw std::runtime_error("Server list JSON is not an array");
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
