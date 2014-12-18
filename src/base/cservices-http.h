#ifndef CSERVICESHTTP_H
#define CSERVICESHTTP_H
//This is a C-only header.
//This header is not standalone, it is included by cservices.h

#include <curl/curl.h>

//struct event;
//struct CURLM;

typedef struct
{
    int start;
    int end;
} t_string_bounds;

typedef struct
{
    int socket;
    short curEvents;
    event* read;
    event* write;
    void (*connOnComplete)(void* self, CURLcode code);
} CurlConnection;

MEGAIO_IMPEXP CURLM* gCurlMultiHandle;
MEGAIO_IMPEXP int services_http_init(unsigned options);
MEGAIO_IMPEXP int services_http_shutdown();
MEGAIO_IMPEXP t_string_bounds services_http_url_get_host(const char* url);

#endif // CSERVICESHTTP_H
