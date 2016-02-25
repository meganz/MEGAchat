#ifndef CSERVICESHTTP_H
#define CSERVICESHTTP_H
//This is a C-only header.
//This header is not standalone, it is included by cservices.h

#include <curl/curl.h>
#ifdef __cplusplus
extern "C" {
#endif

//struct event;
//struct CURLM;

typedef struct
{
    long start;
    long end;
} t_string_bounds;

struct _CurlConnection;
typedef struct _CurlConnection
{
    void (*connOnComplete)(struct _CurlConnection* self, CURLcode code);
} CurlConnection;

extern MEGAIO_IMPEXP CURLM* gCurlMultiHandle;
MEGAIO_IMPEXP int services_http_init(unsigned options);
MEGAIO_IMPEXP int services_http_shutdown();
MEGAIO_IMPEXP int services_http_set_useragent(const char* useragent);
extern MEGAIO_IMPEXP const char* services_http_useragent;
extern MEGAIO_IMPEXP int services_http_use_ipv6;
MEGAIO_IMPEXP t_string_bounds services_http_url_get_host(const char* url);

#ifdef __cplusplus
}
#endif
#endif // CSERVICESHTTP_H
