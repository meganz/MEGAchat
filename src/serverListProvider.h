#ifndef _SERVERLIST_PROVIDER_H
#define _SERVERLIST_PROVIDER_H
#include <string>
#include <vector>
#include <promise.h>

namespace mega { namespace http { class Client; } }

namespace karere
{
struct ServerInfo
{
    std::string host;
    std::string credentials;
    std::string otherInfo;
    unsigned short port;
    ServerInfo(const char* aHost, int aPort, const char* aCredentials="", const char* aOther="")
        :host(aHost), credentials(aCredentials), otherInfo(aOther)
    {
        if (aPort < 0 || aPort > 65535)
            throw std::runtime_error("ServerInfo: Port "+std::to_string(aPort)+" is out of range");
        port = (unsigned short)aPort;
    }
};

class ServerList: public std::vector<std::shared_ptr<ServerInfo> >
{
protected:
    size_t mNextAssignIdx = 0;
    bool needsUpdate() const { return ((mNextAssignIdx >= size()) || empty()); }

};

template <class B>
class SingleServerProvider: public B
{
public:
    using B::B;
    promise::Promise<std::shared_ptr<ServerInfo> > getServer()
    {
        if (B::needsUpdate())
        {
            return B::fetchServers()
            .then([this](int) -> promise::Promise<std::shared_ptr<ServerInfo> >
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
    promise::Promise<std::shared_ptr<ServerList> > getServers()
    {
        if (B::needsUpdate())
        {
            return B::fetchServers()
            .then([this]()
            {
                if (B::needsUpdate())
                    return nullptr;
                B::mNextAssignIdx+=B::size();
                return new ServerList(*this);
            });
        }
        else
        {
            B::mNextAssignIdx+=B::size();
            return std::shared_ptr<ServerList>(new ServerList(*this));
        }
    }
};

class StaticProvider: public ServerList
{
protected:
public:
    StaticProvider(const char* servers);
    promise::Promise<int> fetchServers() { mNextAssignIdx = 0; return 0; }
};

class GelbProvider: public ServerList
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
    promise::Promise<int> fetchServers();
    GelbProvider(const char* gelbHost, const char* service, int64_t maxReuseOldServersAge=0,
        size_t maxAttempts=3, size_t reqTimeout=4000)
    :mGelbHost(gelbHost), mService(service), mReqTimeout(reqTimeout), mMaxAttempts(maxAttempts),
      mMaxReuseOldServersAge(maxReuseOldServersAge)
    {}
};
}
#endif
