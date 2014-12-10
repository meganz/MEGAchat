#include "cservices.h"
#include "gcmpp.h"
#include <event2/dns.h>
#include <event2/util.h>
#include <event2/event.h>
#include <memory.h>
#include <sys/socket.h>
#include <assert.h>
#include <type_traits>
#if __cplusplus < 201103L
   namespace std { typedef unique_ptr auto_ptr; }
#endif

using namespace std;
static_assert(std::is_same<evutil_addrinfo, addrinfo>::value, "evutil_addrinfo is not the same as the system's addrinfo struct");

extern struct event_base* services_eventloop;
struct evdns_base* dnsbase = NULL;

struct DnsRequest: public megaMessage
{
    svcdns_callback cb;
    svcdns_errback errb;
    void* userp;
    unsigned flags;
    int errcode;
    bool canceled;
    megaHandle handle;
    evutil_addrinfo* addr;
    DnsRequest(svcdns_callback aCb, svcdns_errback aErrb, void* aUserp, unsigned aFlags)
        :megaMessage(NULL), cb(aCb), errb(aErrb), userp(aUserp), flags(aFlags),
          addr(NULL), errcode(0), handle(0), canceled(false) {}
    ~DnsRequest()
    {
        if (handle)
            services_hstore_remove_handle(MEGA_HTYPE_DNSREQ, handle);
        if (addr)
            evutil_freeaddrinfo(addr);
    }
};
static inline int toSvcErrCode(int code)
{
    return code; //we have 1:1 matching of libevent to SVC error codes
}

static void msgCallErrback(megaMessage* msg)
{
    unique_ptr<DnsRequest> ud((DnsRequest*)msg);
    ud->errb(toSvcErrCode(ud->errcode), evutil_gai_strerror(ud->errcode), ud->userp);
};

static void msgCallCallback(megaMessage* msg)
{
    unique_ptr<DnsRequest> ud((DnsRequest*)msg);
    int disown_addrinfo = 0;
    ud->cb(ud->addr, ud->userp, &disown_addrinfo);
    if (disown_addrinfo)
        ud->addr = NULL;
}

static inline void callErrback(DnsRequest* req, int errcode)
{
    if (req->flags & SVCF_NO_MARSHALL)
    {
        unique_ptr<DnsRequest> autodel(req);
        req->errb(toSvcErrCode(errcode), evutil_gai_strerror(errcode), req->userp);
    }
    else
    {
        req->errcode = errcode;
        req->func = msgCallErrback;
        megaPostMessageToGui(req);
    }
}

static inline void callCallback(DnsRequest* req, evutil_addrinfo* addr)
{
    if (req->flags & SVCF_NO_MARSHALL)
    {
        unique_ptr<DnsRequest> autodel(req);
        int disown_addrinfo = 0;
        req->cb(addr, req->userp, &disown_addrinfo);
        if (disown_addrinfo)
            req->addr = NULL;
    }
    else
    {
        req->addr = addr;
        req->func = msgCallCallback;
        megaPostMessageToGui(req);
    }
}

void svcdnsCallback(int errcode, evutil_addrinfo* addr, void* userp)
{
    DnsRequest* req = (DnsRequest*)userp;
    if (req->canceled)
    {
        delete req;
        return;
    }
    if (errcode)
        callErrback(req, errcode);
    else
        callCallback(req, addr);
}

MEGAIO_EXPORT int services_dns_init(int options)
{
    dnsbase = evdns_base_new(services_eventloop, 1);
    if (!dnsbase)
    {
        SVCS_LOG_ERROR("evdns_base_new() returned NULL");
        return 0;
    }
    else
    {
        return 1;
    }
}

MEGAIO_EXPORT megaHandle services_dns_lookup(const char* name, const char* service,
    unsigned flags, svcdns_callback cb, svcdns_errback errb, void* userp)
{
    evutil_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
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
    DnsRequest* req = new DnsRequest(cb, errb, userp, flags);
    evdns_getaddrinfo_request* gai =
        evdns_getaddrinfo(dnsbase, name, service, &hints, svcdnsCallback, req);
    if (gai == NULL)
    {
        callErrback(req, DNS_ERR_UNKNOWN);
        return 0;
    }
    return (req->handle = services_hstore_add_handle(MEGA_HTYPE_DNSREQ, req));
}

MEGAIO_EXPORT int services_dns_cancel_lookup(megaHandle handle)
{
    DnsRequest* req = (DnsRequest*)services_hstore_get_handle(MEGA_HTYPE_DNSREQ, handle);
    if (!req)
        return 0;
    req->canceled = true;
    return 1;
}

MEGAIO_EXPORT void services_dns_free_addrinfo(addrinfo* ai)
{
    evutil_freeaddrinfo(ai);
}

MEGAIO_EXPORT const char* services_dns_inet_ntop(int af, const void* src,
    char* dst, socklen_t size)
{
    return evutil_inet_ntop(af, src, dst, size);
}
