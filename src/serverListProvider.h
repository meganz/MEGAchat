#ifndef _SERVERLIST_PROVIDER_H
#define _SERVERLIST_PROVIDER_H
#include <string>
#include <vector>
#include <promise.h>
#include <rapidjson/document.h>
#include "retryHandler.h"

namespace karere
{
#define SRVJSON_CHECK_GET_PROP(varname, name, type)                                             \
    auto it_##varname = json.FindMember(#name);                                                 \
    if (it_##varname == json.MemberEnd())                                                                          \
        throw std::runtime_error("No '" #name "' field in JSON server item");                    \
    if (!it_##varname->value.Is##type())                                                        \
        throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'");            \
    varname = it_##varname->value.Get##type();

#define SRVJSON_GET_OPTIONAL_PROP(varname, name, type)                                           \
    auto it_##name = json.FindMember(#name);                                                     \
    if (it_##name != json.MemberEnd())                                                                               \
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
class ListProvider: public ServerList<S>
{
public: //must be protected, but because of a gcc bug, protected/private members cant be accessed from within a lambda
    typedef ServerList<S> Base;
    size_t mNextAssignIdx = 0;
    bool needsUpdate() const { return ((mNextAssignIdx >= Base::size()) || Base::empty()); }
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
                    return mBase->at(mBase->mNextAssignIdx++);
            });
        }
        else
        {
            return mBase->at(mBase->mNextAssignIdx++);
        }
    }
    std::shared_ptr<typename B::Server> lastServer()
    {
        auto nextIdx = mBase->mNextAssignIdx;
        if (nextIdx <= 0)
            return nullptr;
        return mBase->at(nextIdx-1);
    }
    promise::Promise<ServerList<typename B::Server>*> getServers(unsigned timeout=0)
    {
        if (mBase->needsUpdate())
        {
            return mBase->fetchServers(timeout)
            .then([this]() -> promise::Promise<ServerList<typename B::Server>*>
            {
                if (mBase->needsUpdate())
                    return promise::Error("No servers", 0x3e9a9e1b, 1);
                mBase->mNextAssignIdx += mBase->size();
                return mBase.get();
            });
        }
        else
        {
            mBase->mNextAssignIdx += mBase->size();
            return mBase.get();
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
        doc.Parse(serversJson);
        if (doc.HasParseError())
        {
            throw std::runtime_error("Error "+std::to_string(doc.GetParseError())+
                "parsing json, at position "+std::to_string(doc.GetErrorOffset()));
        }
        parseServerList(doc, *this);
    }
    promise::Promise<void> fetchServers(unsigned timeout=0) { this->mNextAssignIdx = 0; return promise::_Void();}
};

/** An implementation of a server data provider that gets the servers from the GeLB server */
template <class S>
class GelbProvider: public ListProvider<S>, public DeleteTrackable
{
protected:
    MyMegaApi *mApi;
    std::string mService;
    int64_t mMaxReuseOldServersAge;
    int64_t mLastUpdateTs = 0;
    bool started;
    promise::Promise<void> mOutputPromise;
    void parseServersJson(const std::string& json);
    promise::Promise<void> exec(unsigned timeoutms, int maxretries);
    
public:
    typedef S Server;
    promise::Promise<void> fetchServers(unsigned timeoutms = 4000, int maxretries = 1);
    GelbProvider(MyMegaApi *api, const char* service, int64_t maxReuseOldServersAge = 0);
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
    FallbackServerProvider(MyMegaApi *api, const char* service, const char* staticServers,
        int64_t gelbMaxReuseAge=0, int gelbRetryCount=2, unsigned gelbReqTimeout=4000)
        :mGelbProvider(new GelbProvider<S>(api, service, gelbRetryCount, gelbReqTimeout, gelbMaxReuseAge)),
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
    std::shared_ptr<S> lastServer()
    {
        auto server = mGelbProvider.lastServer();
        return server ? server : mStaticProvider.lastServer();
    }
    promise::Promise<ServerList<S>*> getServers(unsigned timeout = 0)
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
GelbProvider<S>::GelbProvider(MyMegaApi *api, const char* service, int64_t maxReuseOldServersAge)
    : mApi(api), mService(service), mMaxReuseOldServersAge(maxReuseOldServersAge),
    started(false)
{
}
    
template <class S>
promise::Promise<void> GelbProvider<S>::exec(unsigned timeoutms, int maxretries)
{
    assert(!started);
    started = true;
    
    return mApi->call(&::mega::MegaApi::queryGeLB, mService.c_str(), timeoutms, maxretries)
    .then([this](ReqResult result)
        -> promise::Promise<void>
    {
        if (result->getNumber() != 200)
        {
            return promise::Error("Non-200 http response from GeLB server: "
                                  +std::to_string(result->getNumber()), 0x3e9a9e1b, 1);
        }
        if (!result->getTotalBytes() || !result->getText())
        {
            return promise::Error("Empty response from GeLB server", 0x3e9a9e1b, 1);
        }

        this->started = false;
        std::string json((const char*)result->getText(), result->getTotalBytes());
        parseServersJson(json);
        this->mNextAssignIdx = 0; //notify about updated servers only if parse didn't throw
        this->mLastUpdateTs = services_get_time_ms();
        return promise::_Void();
    })
    .fail([this](const promise::Error& err)
    {
        started = false;
        return err;
    });
}

template <class S>
promise::Promise<void> GelbProvider<S>::fetchServers(unsigned timeoutms, int maxretries)
{
    if (started)
    {
        return mOutputPromise;
    }

    promise::Promise<void> pms = exec(timeoutms, maxretries);
    mOutputPromise = pms
    .fail([this](const promise::Error& err) -> promise::Promise<void>
    {
        if (!this->mLastUpdateTs || ((services_get_time_ms() - mLastUpdateTs)/1000 > mMaxReuseOldServersAge))
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
        throw std::runtime_error(std::string("Error ")+std::to_string(doc.GetParseError())+
            "parsing json, at position "+std::to_string(doc.GetErrorOffset()));
    }
    auto arr = doc.FindMember(mService.c_str());
    if (arr == doc.MemberEnd())
        throw std::runtime_error("JSON received does not have a '"+mService+"' member");
    try
    {
        parseServerList(arr->value, *this);
    }
    catch (std::exception& e)
    {
        KR_LOG_ERROR("Error parsing GeLB response: JSON dump:\n %s", json.c_str());
        throw;
    }
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
