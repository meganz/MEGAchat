#ifndef _SERVERLIST_PROVIDER_H
#define _SERVERLIST_PROVIDER_H
#include <string>
#include <vector>
#include <promise.h>
#include <rapidjson/document.h>
#include "retryHandler.h"
#include "sdkApi.h"

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
protected:
    void parseServerList(const rapidjson::Value& arr)
    {
        if (!arr.IsArray())
            throw std::runtime_error("Server list JSON is not an array");
        std::vector<std::shared_ptr<karere::TurnServerInfo> > parsed;
        for (auto it=arr.Begin(); it!=arr.End(); ++it)
        {
            if (!it->IsObject())
                throw std::runtime_error("Server info entry is not an object");
            parsed.emplace_back(new karere::TurnServerInfo(*it));
        }

        this->swap(parsed);
    }
};

/** An implementation of a server data provider that gets the servers from a local, static list,
 *  possibly hardcoded
 */
class StaticProvider: public ListProvider<TurnServerInfo>
{
protected:
public:
    StaticProvider(const char* serversJson)
    {
        rapidjson::Document doc;
        doc.Parse(serversJson);
        if (doc.HasParseError())
        {
            throw std::runtime_error("Error "+std::to_string(doc.GetParseError())+
                "parsing json, at position "+std::to_string(doc.GetErrorOffset()));
        }

        parseServerList(doc);
    }
};

/** An implementation of a server data provider that gets the servers from the GeLB server */
class GelbProvider: public ListProvider<karere::TurnServerInfo>, public DeleteTrackable
{
protected:
    MyMegaApi& mApi;
    std::string mService;
    int64_t mMaxReuseOldServersAge;
    int64_t mLastUpdateTs = 0;
    bool mBusy = false;
    promise::Promise<void> mOutputPromise;
    void parseServersJson(const std::string& json)
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
            parseServerList(arr->value);
        }
        catch (std::exception& e)
        {
            KR_LOG_ERROR("Error parsing GeLB response: JSON dump:\n %s", json.c_str());
            throw;
        }
    }

public:
    promise::Promise<void> fetchServers()
    {
        if (mBusy)
        {
            return mOutputPromise;
        }

        mBusy = true;

        return mApi.call(&::mega::MegaApi::queryGeLB, mService.c_str(), 10000, 2)
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

            mBusy = false;
            std::string json((const char*)result->getText(), result->getTotalBytes());
            parseServersJson(json);
            this->mLastUpdateTs = services_get_time_ms();
            return promise::_Void();
        })
        .fail([this](const promise::Error& err)
        {
            mBusy = false;
            return err;
        });
    }

    GelbProvider(MyMegaApi& api, const char* service, int64_t maxReuseOldServersAge = 0)
        : mApi(api), mService(service), mMaxReuseOldServersAge(maxReuseOldServersAge)
    {
    }
};
}

#endif
