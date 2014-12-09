#ifndef CSERVICESDNS_H
#define CSERVICESDNS_H
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

typedef void(*svcdns_callback)(struct addrinfo* addrs, void* userp);
typedef void(*svcdns_errback)(int errcode, const char* errmsg, void* userp);

#endif // CSERVICESDNS_H
