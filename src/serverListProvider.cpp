#include "serverListProvider.h"
#include <base/services-http.hpp>
#include <rapidjson/document.h>
#include "retryHandler.h"

namespace karere
{
promise::Promise<int> GelbProvider::fetchServers()
{
    std::shared_ptr<mega::http::Client> client(new mega::http::Client);
    return ::mega::retry([this, client](int no)
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
            mNextAssignIdx = 0; //notify about updated servers only if parse didn't throw
            mLastUpdateTs = timestampMs();
            return 0;
        });
    }, [this, client](){client->abort();}, mReqTimeout, mMaxAttempts, 0)
    .fail([this](const promise::Error& err) -> promise::Promise<int>
    {
        if (mLastUpdateTs && ((timestampMs() - mLastUpdateTs) < mMaxReuseOldServersAge))
        {
            mNextAssignIdx = 0;
            KR_LOG_WARNING("Gelb client: error getting new servers, reusing not too old servers that we have from gelb");
            return 0;
        }
        else
            return err;
    });
}

#define CHECK_GET_SERVER_PROP(name, type)                                             \
    auto name = it->FindMember(#name);                                                \
    if (name == it->MemberEnd())                                                        \
        throw std::runtime_error("No '" #name "' field in JSON server item");         \
    if (!name->value.Is##type())                                                              \
        throw std::runtime_error("JSON field '" #name "' is not of type '" #type "'")


static inline const char* strOrEmpty(const char* str)
{
    if (!str)
        return "";
    else
        return str;
}

void GelbProvider::parseServerList(const std::string& json)
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
        CHECK_GET_SERVER_PROP(host, String);
        CHECK_GET_SERVER_PROP(port, Int);
        this->emplace_back(new ServerInfo(host->value.GetString(), port->value.GetInt()));
    }
}
}
