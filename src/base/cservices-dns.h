#ifndef CSERVICESDNS_H
#define CSERVICESDNS_H
//This header is not standalone. It is included by cservices.h
//This is a plain C header
#include "cservices.h"
#include "event2/dns.h"
enum
{
/** No error */
    SVCDNS_ENONE = DNS_ERR_NONE,
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
    SVCF_DNS_UDP_ADDR = SVCF_LAST << 1
};

typedef void(*svcdns_callback)(struct addrinfo* addrs, void* userp, int* disown_addrinfo);
typedef void(*svcdns_errback)(int errcode, const char* errmsg, void* userp);
MEGAIO_IMPEXP int services_dns_init(int options);
MEGAIO_IMPEXP megaHandle services_dns_lookup(const char* name, const char* service,
    unsigned flags, svcdns_callback cb, svcdns_errback errb, void* userp);

MEGAIO_IMPEXP int services_dns_cancel_lookup(megaHandle handle);
MEGAIO_IMPEXP void services_dns_free_addrinfo(struct addrinfo* addr);
MEGAIO_IMPEXP const char* services_dns_inet_ntop(int af, const void* src,
    char* dst, socklen_t size);
#endif // CSERVICESDNS_H
