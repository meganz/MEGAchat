#ifndef SERVICES_DNS_HPP
#define SERVICES_DNS_HPP
//This header is the C++11 layer on top of cservices-dns.h
#include "cservices-dns.h"
#include "addrinfo.hpp"
#include "promise.h"

namespace mega
{
enum {kDnsPromiseError = 0x3e9ad115}; //should resemble 'megadns'

template <class CB>
struct DnsReqMsg: public megaMessage
{
    CB cb;
    int errcode;
    evutil_addrinfo* addr;
    DnsReqMsg(CB&& aCb)
        :megaMessage(gcmFunc), cb(std::forward<CB>(aCb)), errcode(0), addr(nullptr){}
protected:
    static void gcmFunc(megaMessage* msg)
    {
        DnsReqMsg<CB>* self = (DnsReqMsg<CB>*)msg;
        if (self->errcode)
        {
            self->cb(services_dns_backend_to_svc_errcode(self->errcode),
                     std::shared_ptr<AddrInfo>());
        }
        else
        {
            self->cb(0, std::shared_ptr<AddrInfo>(new AddrInfo(self->addr, true)));
        }
    }
};

template <class CB>
void dnsLookup(const char* name, unsigned flags, CB&& cb, const char* service=nullptr)
{
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

    auto msg = new DnsReqMsg<CB>(std::forward<CB>(cb));
    evdns_getaddrinfo(services_dns_eventbase, name, service, &hints,
        [](int errcode, evutil_addrinfo* addr, void* userp)
        {
            auto msg = (DnsReqMsg<CB>*)userp;
            msg->errcode = errcode;
            msg->addr = addr;
            megaPostMessageToGui(msg);
        }, msg);
}

promise::Promise<std::shared_ptr<AddrInfo> >
dnsLookup(const char* name, unsigned flags, const char* service=nullptr)
{
    promise::Promise<std::shared_ptr<AddrInfo> > pms;
    dnsLookup(name, flags,
        [pms](int errcode, std::shared_ptr<AddrInfo>&& addrs) mutable
    {
        if (!errcode)
            pms.resolve(std::forward<std::shared_ptr<AddrInfo> >(addrs));
        else
            pms.reject(promise::Error(nullptr, errcode, kDnsPromiseError));
    }, service);
    return pms;
}
}
#endif // SERVICES_DNS_HPP
