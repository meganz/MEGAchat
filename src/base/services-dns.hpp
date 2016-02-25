#ifndef SERVICES_DNS_HPP
#define SERVICES_DNS_HPP
//This header is the C++11 layer on top of cservices-dns.h
#include "cservices.h"
#include "addrinfo.hpp"
#include "promise.h"
#include "gcmpp.h"
#include <string.h>

namespace mega
{
enum {ERRTYPE_DNS = 0x3e9ad115}; //should resemble 'megadns'

class DnsCache;
extern DnsCache gDnsCache;

template <class CB>
struct DnsReqMsg: public megaMessage
{
    CB cb;
    int errcode = 0;
    std::shared_ptr<AddrInfo> addr;
    std::string domain;
    DnsReqMsg(CB&& aCb, std::string&& aDomain)
    :megaMessage(gcmFunc), cb(std::forward<CB>(aCb)),
         domain(std::forward<std::string>(aDomain)){}
protected:
    static void gcmFunc(void* msg)
    {
        DnsReqMsg<CB>* self = (DnsReqMsg<CB>*)msg;
        if (self->errcode)
        {
            self->cb(services_dns_backend_to_svc_errcode(self->errcode), self->addr);
        }
        else
        {
            self->cb(0, self->addr);
        }
    }
};

struct DnsCacheKey
{
    std::string domain;
    bool isIp6;
    DnsCacheKey(const std::string& aDomain, bool aIsIp6): domain(aDomain), isIp6(aIsIp6){}
    DnsCacheKey(DnsCacheKey&& other): domain(std::move(other.domain)), isIp6(other.isIp6){}
    bool operator<(const DnsCacheKey& other) const
    {
        if (isIp6 != other.isIp6)
            return isIp6; //consider any ipv6 smaller than any ipv4
        else
            return domain < other.domain;
    }
    bool operator==(const DnsCacheKey& other) const
    { return isIp6 == other.isIp6 && domain == other.domain; }
};

class DnsCacheItem: public AddrInfo
{
public:
    int32_t ts;
    DnsCacheItem(const AddrInfo& addrs): AddrInfo(addrs), ts(time(NULL)){}
    DnsCacheItem(const std::shared_ptr<Ipv4List>& ip4):ts(time(NULL)){ mIpv4Addrs = ip4; }
    DnsCacheItem(const std::shared_ptr<Ipv6List>& ip6):ts(time(NULL)){ mIpv6Addrs = ip6; }
    void reset4(const std::shared_ptr<Ipv4List>& ip4)
    {
        ts = time(NULL);
        mIpv4Addrs = ip4;
    }
    void reset6(const std::shared_ptr<Ipv6List>& ip6)
    {
        ts = time(NULL);
        mIpv6Addrs = ip6;
    }
};

class DnsCache: protected std::map<DnsCacheKey, std::shared_ptr<DnsCacheItem>>
{
    std::mutex mMutex;
public:
    unsigned mMaxTTL = 3600;
    std::shared_ptr<DnsCacheItem> lookup(const char* name, bool isIp6)
    {
        assert(name);
        std::lock_guard<std::mutex> locker(mMutex);
        auto it = find(DnsCacheKey(name, isIp6));
        if (it == end())
            return nullptr;
        auto& item = it->second;
        if (time(NULL) - item->ts > mMaxTTL)
        {
            erase(it);
            return nullptr;
        }
        else
        {
            return item;
        }
    }
    std::shared_ptr<AddrInfo::Ipv4List> lookup4(const char* name)
    {
        return lookup(name, false)->ip4addrs();
    }
    std::shared_ptr<AddrInfo::Ipv6List> lookup6(const char* name)
    {
        return lookup(name, true)->ip6addrs();
    }


    void put(const std::string& name, const std::shared_ptr<AddrInfo>& addr)
    {
        std::lock_guard<std::mutex> locker(mMutex);
        if (addr->ip4addrs())
        {
            DnsCacheKey key(name, false);
            auto it = find(key);
            if (it != end())
                it->second->reset4(addr->ip4addrs());
            else
                emplace(std::move(key), std::make_shared<DnsCacheItem>(*addr));
        }
        if (addr->ip6addrs())
        {
            DnsCacheKey key(name, true);
            auto it = find(key);
            if (it != end())
                it->second->reset6(addr->ip6addrs());
            else
                emplace(std::move(key), std::make_shared<DnsCacheItem>(*addr));
        }
    }
};

template <class CB>
static inline void dnsLookup(const char* name, unsigned flags, CB&& cb, const char* service=nullptr)
{
    if (!name)
        throw std::runtime_error("dnsLookup: NULL name provided");

    auto cached = gDnsCache.lookup(name, flags & SVCF_DNS_IPV6);
    if (cached)
    {
        SVC_LOG_DEBUG("DNS cache hit for domain %s", name);
        ::mega::marshallCall([cached, cb]() mutable{cb(0, cached); });
        return;
    }
    evutil_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    if (flags & SVCF_DNS_IPV6)
        hints.ai_family = AF_INET6;
    else if (flags & SVCF_DNS_IPV4)
        hints.ai_family = AF_INET;
    else
        hints.ai_family = AF_UNSPEC;
    hints.ai_flags = EVUTIL_AI_CANONNAME;
/* Unless we specify a socktype, we'll get at least two entries:
 * one for TCP and one for UDP. That's not what we want. */
    if (flags & SVCF_DNS_UDP_ADDR)
    {
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
    }
    else
    {
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }
    auto msg = new DnsReqMsg<CB>(std::forward<CB>(cb), name);
    evdns_getaddrinfo(services_dns_eventbase, name, service, &hints,
        [](int errcode, evutil_addrinfo* addr, void* userp)
        {
            auto msg = (DnsReqMsg<CB>*)userp;
            if (errcode)
            {
                msg->errcode = errcode;
            }
            else
            {
                auto spAddr = std::make_shared<ParsingAddrInfo>(addr);
                msg->addr = spAddr;
                gDnsCache.put(msg->domain, std::static_pointer_cast<AddrInfo>(spAddr));
            }
            megaPostMessageToGui(msg);
        }, msg);
}

static inline promise::Promise<std::shared_ptr<AddrInfo> >
dnsLookup(const char* name, unsigned flags, const char* service=nullptr)
{
    promise::Promise<std::shared_ptr<AddrInfo> > pms;
    dnsLookup(name, flags,
        [pms](int errcode, const std::shared_ptr<AddrInfo>& addrs) mutable
    {
        if (!errcode)
            pms.resolve(addrs);
        else
            pms.reject(promise::Error(nullptr, errcode, ERRTYPE_DNS));
    }, service);
    return pms;
}
}
#endif // SERVICES_DNS_HPP
