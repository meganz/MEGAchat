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

extern struct event_base* services_eventloop;
struct evdns_base* dnsbase = NULL;

struct UserData: public megaMessage
{
    svcdns_callback cb;
    svcdns_errback errb;
    void* userp;
    unsigned flags;
    int errcode;
    megaHandle handle;
    evutil_addrinfo* addr;
    UserData(svcdns_callback aCb, svcdns_errback aErrb, void* aUserp, unsigned aFlags)
        :megaMessage(NULL), cb(aCb), errb(aErrb), userp(aUserp), flags(aFlags),
          addr(NULL), errcode(0), handle(0) {}
    ~UserData()
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
    auto_ptr<UserData> ud((UserData*)msg);
    ud->errb(toSvcErrCode(ud->errcode), evutil_gai_strerror(ud->errcode), ud->userp);
};

static void msgCallCallback(megaMessage* msg)
{
    auto_ptr<UserData> ud((UserData*)msg);
    ud->cb(ud->addr, ud->userp);
}

static inline void callErrback(UserData* userdata, int errcode)
{
    if (userdata->flags & SVCF_NO_MARSHALL)
    {
        userdata->errb(toSvcErrCode(errcode), evutil_gai_strerror(errcode), userdata->userp);
        delete userdata;
    }
    else
    {
        userdata->errcode = errcode;
        userdata->func = msgCallErrback;
        megaPostMessageToGui(userdata);
    }
}

static inline void callCallback(UserData* userdata, evutil_addrinfo* addr)
{
    if (userdata->flags & SVCF_NO_MARSHALL)
    {
        userdata->cb(addr, userdata->userp);
        delete userdata;
    }
    else
    {
        userdata->addr = addr;
        userdata->func = msgCallCallback;
        megaPostMessageToGui(userdata);
    }
}

void svcdnsCallback(int errcode, evutil_addrinfo* addr, void* userp)
{
    UserData* userdata = (UserData*)userp;
    if (errcode)
        callErrback(userdata, errcode);
    else
        callCallback(userdata, addr);
}

int services_dns_init(int options)
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

megaHandle services_dns_lookup(const char* name, const char* service,
    unsigned flags, svcdns_callback cb, svcdns_errback errb, void* userp)
{
    evutil_addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = EVUTIL_AI_CANONNAME;
/* Unless we specify a socktype, we'll get at least two entries:
 * one for TCP and one for UDP. That's not what we want. */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    UserData* userdata = new UserData(cb, errb, userp, flags);
    struct evdns_getaddrinfo_request *req =
        evdns_getaddrinfo(dnsbase, name, service, &hints, svcdnsCallback, userp);
    if (req == NULL)
    {
        callErrback(userdata, DNS_ERR_UNKNOWN);
        return 0;
    }
    return userdata->handle = services_hstore_add_handle(MEGA_HTYPE_DNSREQ, req);
}
