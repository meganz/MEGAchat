#ifndef ADDRINFO_HPP
#define ADDRINFO_HPP
#include "cservices.h"
#include <vector>
#ifndef _WIN32
    #include <netinet/ip.h>
#endif
namespace mega
{
template <int AF, typename Raw, int SLen>
class IpAddr
{
protected:
    Raw mAddr;
    mutable char* mBuf;
public:
    IpAddr(const Raw& addr): mAddr(addr), mBuf(NULL){}
    ~IpAddr()
    {
        if(mBuf)
            ::free(mBuf);
    }
    const Raw& raw() const {return mAddr;}
    const char* toString() const
    {
        if (mBuf)
            return mBuf;
        mBuf = (char*)malloc(SLen);
        if (!mBuf)
            throw std::runtime_error("IpAddr::toString: Out of memory");
        if (!services_dns_inet_ntop(AF, &mAddr, mBuf, SLen))
            throw std::runtime_error("Error converting ip address to string");
        return mBuf;
    }
};

typedef IpAddr<AF_INET, in_addr, 16> Ipv4Addr;
typedef IpAddr<AF_INET6, in6_addr, 48> Ipv6Addr;


class AddrInfo
{
public:
    typedef std::vector<Ipv4Addr> Ipv4List;
    typedef std::vector<Ipv6Addr> Ipv6List;
    const std::shared_ptr<Ipv4List>& ip4addrs() const {return mIpv4Addrs;}
    const std::shared_ptr<Ipv6List>& ip6addrs() const {return mIpv6Addrs;}
    const std::string& canonName() const {return mCanonName;}
    AddrInfo(){}
    template<class Other>
    AddrInfo(Other&& other): mIpv4Addrs(other.mIpv4Addrs), mIpv6Addrs(other.mIpv6Addrs),
        mCanonName(std::forward<Other>(other.mCanonName)){}
    virtual ~AddrInfo() {}
protected:
    std::shared_ptr<Ipv4List> mIpv4Addrs;
    std::shared_ptr<Ipv6List> mIpv6Addrs;
    std::string mCanonName;
};

class ParsingAddrInfo: public AddrInfo
{
public:
    ParsingAddrInfo(addrinfo* ai, bool ownAi=false): mAi(ownAi?ai:nullptr)
    {
        if (ai->ai_canonname)
            mCanonName = ai->ai_canonname;
        while(ai)
        {
            if (ai->ai_family == AF_INET)
            {
                if (!mIpv4Addrs)
                    mIpv4Addrs = std::make_shared<Ipv4List>();
                 mIpv4Addrs->emplace_back(((sockaddr_in*)ai->ai_addr)->sin_addr);
            }
            else if (ai->ai_family == AF_INET6)
            {
                if (!mIpv6Addrs)
                    mIpv6Addrs = std::make_shared<Ipv6List>();
                 mIpv6Addrs->emplace_back(((sockaddr_in6*)ai->ai_addr)->sin6_addr);
            }
            else
            {
                 SVC_LOG_ERROR("DNS: Unknown family of address returned by dns resolver");
            }
            ai=ai->ai_next;
        }
    }
    ~ParsingAddrInfo()
    {
        if (mAi)
            services_dns_free_addrinfo(mAi);
    }
    addrinfo* getAddrinfo() const {return mAi;}
protected:
    addrinfo* mAi;
};

}
#endif // ADDRINFO_HPP
