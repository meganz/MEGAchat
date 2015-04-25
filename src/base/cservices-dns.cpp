#include "cservices.h"
#include "gcmpp.h"
#include <event2/dns.h>
#include <event2/util.h>
#include <event2/event.h>
#include <memory.h>
#include <sys/socket.h>
#include <assert.h>
#include <type_traits>

using namespace std;
static_assert(std::is_same<evutil_addrinfo, addrinfo>::value, "evutil_addrinfo is not the same as the system's addrinfo struct");

extern "C" struct event_base* services_eventloop;
struct evdns_base* services_dns_eventbase = NULL;

MEGAIO_EXPORT int services_dns_init(int options)
{
    services_dns_eventbase = evdns_base_new(services_eventloop, 1);
    if (!services_dns_eventbase)
    {
        SVC_LOG_ERROR("evdns_base_new() returned NULL");
        return 0;
    }
    else
    {
        return 1;
    }
}

MEGAIO_EXPORT const char* services_dns_inet_ntop(int af, const void* src,
    char* dst, socklen_t size)
{
    return evutil_inet_ntop(af, src, dst, size);
}

MEGAIO_EXPORT int services_dns_host_type(const char* host, const char* end)
{
    bool hex = false;
    int colon = false;
    int dot = 0;
    const char* p = host;
    if (*p == '[') //address in square brackets
        p++;
    for (;*p && p<end; p++)
    {
        char ch = *p;
        if ((ch >= 0) && (ch <= 9))
        {
            ;
        }
        else if (((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F')))
        {
            if (dot)
                return SVC_DNS_HOST_DOMAIN;
            hex = true;
        }
        else if (ch == ':')
        {
            if (dot)
                return SVC_DNS_HOST_INVALID;
            return SVC_DNS_HOST_IPV6; //could still be invalid
        }
        else if (ch == '.')
        {
            if (colon)
                return SVC_DNS_HOST_INVALID;
            if (hex) //dots + a-f: must be domain name
                return SVC_DNS_HOST_DOMAIN;
            dot++;
        }
        else if (ch == ']') //closing bracket
        {
            if ((p < end-1) && (*(p+1))) //not the last char
                    return SVC_DNS_HOST_INVALID;
            else break;
        }
        else //any other letter - must be domain
        {
            if (colon)
                return SVC_DNS_HOST_INVALID;
            return SVC_DNS_HOST_DOMAIN;
        }
    }
    //we have only decimal digits and dots, this can't be a domain (no TLD with only digits)
    if (dot && (dot != 3))
        return SVC_DNS_HOST_INVALID;
    //We don't check if the numbers are 0-255
    return SVC_DNS_HOST_IPV4;
}
