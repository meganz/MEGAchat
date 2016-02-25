#ifndef CSERVICESDNS_H
#define CSERVICESDNS_H
//This header is not standalone. It is included by cservices.h
//This is a plain C header
#include "cservices.h"
#include "event2/dns.h"
enum
{
/** No error */
    SVCDNS_ESUCCESS = DNS_ERR_NONE,
/** The name server was unable to interpret the query.*/
    SVCDNS_EFORMAT = DNS_ERR_FORMAT,
/** The name server was unable to process this query due to a problem with the name server. */
    SVCDNS_ESERVERFAILED = DNS_ERR_SERVERFAILED,
/** The domain name does not exist. */
    SVCDNS_ENOTEXIST = DNS_ERR_NOTEXIST,
/** The name server does not support the requested kind of query. */
    SVCDNS_ENOTIMPL = DNS_ERR_NOTIMPL,
/** The name server refuses to perform the specified operation for policy reasons */
    SVCDNS_EREFUSED = DNS_ERR_REFUSED,
/** The reply was truncated or ill-formatted. */
    SVCDNS_ETRUNCATED = DNS_ERR_TRUNCATED,
/** An unknown error occurred */
    SVCDNS_EUNKNOWN = DNS_ERR_UNKNOWN,
/** Communication with the server timed out. */
    SVCDNS_ETIMEOUT = DNS_ERR_TIMEOUT,
/** The request was canceled because the DNS subsystem was shut down. */
    SVCDNS_ESHUTDOWN = DNS_ERR_SHUTDOWN,
 /** The request was canceled by the user. Not used currently */
    SVCDNS_ECANCEL = DNS_ERR_CANCEL,
 /** There were no answers and no error condition in the DNS packet. */
    SVCDNS_ENODATA = DNS_ERR_NODATA
};
enum
{
    SVCF_DNS_UDP_ADDR = SVCF_LAST << 1,
    SVCF_DNS_IPV4 = SVCF_LAST << 2,
    SVCF_DNS_IPV6 = SVCF_LAST << 3
};
#ifdef __cplusplus
extern "C" {
#endif

extern MEGAIO_IMPEXP struct evdns_base* services_dns_eventbase;
static inline int services_dns_backend_to_svc_errcode(int code)
{
    return code; //we have 1:1 matching of libevent to SVC error codes
}
static inline const char* services_dns_errcode_to_string(int errcode)
{
    return evutil_gai_strerror(errcode);
}

MEGAIO_IMPEXP int services_dns_init(int options);
MEGAIO_IMPEXP const char* services_dns_inet_ntop(int af, const void* src,
    char* dst, socklen_t size);

/** Returns values of services_dns_host_type */
enum
{
    SVC_DNS_HOST_DOMAIN = 0,
    SVC_DNS_HOST_INVALID = 1,
    SVC_DNS_HOST_IS_IP = 2,
    SVC_DNS_HOST_IPV4 = SVC_DNS_HOST_IS_IP | 0,
    SVC_DNS_HOST_IPV6 = SVC_DNS_HOST_IS_IP | 1
};

MEGAIO_IMPEXP int services_dns_host_type(const char* host, const char* end);

static inline void services_dns_free_addrinfo(addrinfo* ai)
{
    evutil_freeaddrinfo(ai);
}

#ifdef __cplusplus
}
#endif

#endif // CSERVICESDNS_H
