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
            KR_LOG_DEBUG("TURN server url missing 'turn:' prefix, adding it");
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

// A list of TurnServerInfo structures
using ServerList = std::vector<std::shared_ptr<TurnServerInfo> >;

/** An abstract class that provides a list of servers (i.e. more than one). A single server info is
 * contained in the class S
 */
class ListProvider: public ServerList
{
protected:
    bool parseServerList(const rapidjson::Value& arr)
    {
        if (!arr.IsArray())
        {
            KR_LOG_ERROR("Ice server list JSON is not an array");
            return false;
        }

        std::vector<std::shared_ptr<karere::TurnServerInfo> > parsed;
        for (auto it=arr.Begin(); it!=arr.End(); ++it)
        {
            if (!it->IsObject())
            {
                KR_LOG_ERROR("Ice server info entry is not an object");
                return false;
            }

            parsed.emplace_back(new karere::TurnServerInfo(*it));
        }

        this->swap(parsed);

        return true;
    }
};

/** An implementation of a list provider that gets the servers from a local, static list,
 *  possibly hardcoded
 */
class StaticProvider: public ListProvider
{
protected:
public:
    StaticProvider(const char* serversJson)
    {
        rapidjson::Document doc;
        doc.Parse(serversJson);
        if (doc.HasParseError())
        {
            KR_LOG_ERROR("Error parse static ice-server: %d parsing json, at position %d",
                           doc.GetParseError(),
                           doc.GetErrorOffset());
        }

        if (!parseServerList(doc))
        {
            KR_LOG_ERROR("Error to extract server form static ice-server list");
        }

    }
};

/** An implementation of a list provider that gets the servers from the GeLB server */
class GelbProvider: public ListProvider, public DeleteTrackable
{
protected:
    MyMegaApi& mApi;
    std::string mService;
    bool mBusy = false;
    promise::Promise<void> mOutputPromise;
    bool parseServersJson(const std::string& json)
    {
        rapidjson::Document doc;
        doc.Parse<0>(json.c_str());
        if (doc.HasParseError())
        {
            return false;
        }
        auto arr = doc.FindMember(mService.c_str());
        if (arr == doc.MemberEnd())
        {
            return false;
        }

        return parseServerList(arr->value);
    }

public:
    promise::Promise<void> fetchServers()
    {
        if (mBusy)
        {
            return mOutputPromise;
        }

        mBusy = true;

        auto wptr = weakHandle();
        mOutputPromise = mApi.call(&::mega::MegaApi::queryGeLB, mService.c_str(), 0, 0)
        .then([wptr, this](ReqResult result)
            -> promise::Promise<void>
        {
            if (wptr.deleted())
                return promise::_Void();

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
            std::string json((const char*)result->getText(), (size_t)result->getTotalBytes());
            if (!parseServersJson(json))
            {
                return promise::Error("Data from GeLB server incorrect: " + json, 0x3e9a9e1b, 1);
            }
            return promise::_Void();
        })
        .fail([this](const promise::Error& err)
        {
            mBusy = false;
            return err;
        });

        return mOutputPromise;
    }

    GelbProvider(MyMegaApi& api, const char* service)
        : mApi(api), mService(service)
    {
    }
};
}

#endif
