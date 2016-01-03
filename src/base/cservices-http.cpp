#include "cservices.h"
#include <curl/curl.h>
#include <event2/event.h>
#include <assert.h>
#include "gcmpp.h"
#include <string.h>

#define always_assert(cond) \
    if (!(cond)) SVC_LOG_ERROR("HTTP: Assertion failed: '%s' at file %s, line %d", #cond, __FILE__, __LINE__)
#undef SVC_HTTP_DEBUG_LIBEVENT_BRIDGE
#ifdef SVC_HTTP_DEBUG_LIBEVENT_BRIDGE
    #define LE2CURL_LOG(fmtString,...) printf("libevent-curl: " fmtString "\n", ##__VA_ARGS__)
#else
    #define LE2CURL_LOG(fmtString,...)
#endif


event* gTimerEvent = NULL;
MEGAIO_EXPORT CURLM* gCurlMultiHandle = NULL;
int gNumRunning = 0;
MEGAIO_EXPORT const char* services_http_useragent = NULL;
MEGAIO_EXPORT int services_http_use_ipv6 = 0;

static void le2_onEvent(int fd, short events, void* userp);
static inline void checkCompleted();
static inline short curlToLe2Events(int what);
//static inline const char *le2EventsToString(short events);

struct CurlEvents
{
    event* read;
    event* write;
    short curEvents;
    int sock; //only needed for logging/debugging
    CurlEvents(int sockfd)
        :read(event_new(services_get_event_loop(), sockfd, EV_READ|EV_PERSIST, le2_onEvent, this)),
         write(event_new(services_get_event_loop(), sockfd, EV_WRITE|EV_PERSIST, le2_onEvent, this)),
         curEvents(0), sock(sockfd)
    {}
    ~CurlEvents()
    {
        event_del(read);
        event_del(write);
        event_free(read);
        event_free(write);
    }
    void subscribeToEvents(int what)
    {
        short events = curlToLe2Events(what);
        short changed = events ^ curEvents;
        curEvents = events;
        LE2CURL_LOG("Subscribe socket %d to events: %s", sock, le2EventsToString(events));
        if (changed & EV_READ) //change requested on read subscription
        {
            if (events & EV_READ)
                event_add(read, nullptr);
            else
                event_del(read);
        }
        if (changed & EV_WRITE)
        {
            if (events & EV_WRITE)
                event_add(write, nullptr);
            else
                event_del(write);
        }
    }
};

static inline short curlToLe2Events(int what)
{
    short ret = (what & CURL_POLL_IN)?EV_READ:0;
    if (what & CURL_POLL_OUT)
        ret |= EV_WRITE;
    return ret;
}

static inline int le2ToCurlEvents(short events)
{
    int ret = (events & EV_READ)?CURL_CSELECT_IN:0;
    if (events & EV_WRITE)
        ret|=CURL_CSELECT_OUT;
    return ret;
}
/*
static inline const char* le2EventsToString(short events)
{
    if ((events & (EV_READ|EV_WRITE)) == (EV_READ|EV_WRITE))
        return "READ|WRITE";
    else if (events & EV_READ)
        return "READ";
    else if (events & EV_WRITE)
        return "WRITE";
    else
        return "(NONE)";
}
*/
static void le2_onEvent(int fd, short events, void* userp)
{
    LE2CURL_LOG("Event %s on socket %d", le2EventsToString(events), fd);
    events = le2ToCurlEvents(events);
//    int oldNumRunning = gNumRunning;
    int ret = curl_multi_socket_action(gCurlMultiHandle, fd, events, &gNumRunning);
    if (ret != CURLM_OK)
    {
        SVC_LOG_ERROR("le2_onEvent: curl_multi_socket_action() returned error %d", ret);
        return;
    }
    checkCompleted();
}

static void le2_onTimer(int fd, short kind, void *userp)
{
    LE2CURL_LOG("Timer event");
//    int oldNumRunning = gNumRunning;
    int ret = curl_multi_socket_action(gCurlMultiHandle,
        CURL_SOCKET_TIMEOUT, 0, &gNumRunning);
    if (ret != CURLM_OK)
    {
        SVC_LOG_ERROR("le2_onTimer: curl_multi_socket_action() returned error %d", ret);
        return;
    }
    checkCompleted();
}

static void checkCompleted()
{
    CURLMsg* msg;
    int msgsLeft;
    while ((msg = curl_multi_info_read(gCurlMultiHandle, &msgsLeft)))
    {
        if (msg->msg == CURLMSG_DONE)
        {
            auto easy = msg->easy_handle;
            auto res = msg->data.result;
            CurlConnection* conn;
            curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
            curl_multi_remove_handle(gCurlMultiHandle, easy);
            mega::marshallCall([conn, res](){conn->connOnComplete(conn, res);});
        }
    }
}

static int curlcb_subscribe_to_events(CURL *e, curl_socket_t fd, int what, void *cbp, void *sockp)
{
    CurlEvents* evts = (CurlEvents*)sockp;
    if (what == CURL_POLL_REMOVE)
    {
        if (evts)
        {
            LE2CURL_LOG("Removing events struct from socket %d (easy handle: %p)", fd, e);
            curl_multi_assign(gCurlMultiHandle, fd, NULL);
            delete evts;
        }
    }
    else
    {
        if (!evts)
        {
            LE2CURL_LOG("Creating events struct on socket %d: (easy handle: %p)", fd, e);
            evts = new CurlEvents(fd);
            curl_multi_assign(gCurlMultiHandle, fd, evts);
        }
        evts->subscribeToEvents(what);
    }
    return 0;
}

static int curlcb_subscribe_to_timer(CURLM *multi, long timeout_ms, void* userp)
{
    if (timeout_ms < 0)
        return 0;
    LE2CURL_LOG("Subscribe to timer: %ld", timeout_ms);
    struct timeval timeout;
    timeout.tv_sec = timeout_ms/1000;
    timeout.tv_usec = (timeout_ms%1000)*1000;
    evtimer_add(gTimerEvent, &timeout);
    return 0;
}


MEGAIO_EXPORT int services_http_init(unsigned options)
{
    if (gCurlMultiHandle)
        return -1;
    services_http_set_useragent("Mega Client");
    curl_global_init(CURL_GLOBAL_ALL);
    gCurlMultiHandle = curl_multi_init();
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_SOCKETFUNCTION, curlcb_subscribe_to_events);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_SOCKETDATA, NULL);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_TIMERFUNCTION, curlcb_subscribe_to_timer);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_TIMERDATA, NULL);
    gTimerEvent = evtimer_new(services_get_event_loop(), le2_onTimer, NULL);
    return 0;
}

MEGAIO_EXPORT int services_http_shutdown()
{
    if (!gCurlMultiHandle)
        return -1;
    event_del(gTimerEvent);
    event_free(gTimerEvent);
    gTimerEvent = NULL;
    curl_multi_cleanup(gCurlMultiHandle);
    gCurlMultiHandle = NULL;
    return 0;
}

MEGAIO_EXPORT int services_http_set_useragent(const char* useragent)
{
    size_t len = strlen(useragent);
    if (len < 1)
    {
        SVC_LOG_ERROR("services_http_set_useragent: Attempt to assing an empty user agent");
        return 0;
    }
    if (services_http_useragent)
        free((void*)services_http_useragent);
    services_http_useragent = (const char*)malloc(len+1);
    memcpy((void*)services_http_useragent, useragent, len+1);
    return 1;
}

static const char* url_find_host_end(const char* p);
MEGAIO_EXPORT t_string_bounds services_http_url_get_host(const char* url)
{
    const char* p = url;
    for (; *p; p++)
    {
        char ch = *p;
        if (ch == '/')
            break;
    }
    if (!*p) //no slash till end of url - assume whole url is a host
        return {0, p-url};
    if ((p-url < 2) || !(*(p+1))) // 'x://' requires offset of at least 2 of first slash, and must have at least the next slash char
        return {-1,-1};

    if ((*(p-1) == ':') && (*(p+1) == '/')) //we are at the first slash of xxx://
    {
        p += 2; //go to the start of the host
        const char* end = url_find_host_end(p);
        if (!end)
            return {-1,-1};
        else
            return {p-url, end-url};
    }
    else
    {
        const char* end = url_find_host_end(url);
        if (!end)
            return {-1, -1};
        else
            return {0, end-url};
    }
}
const char* url_find_host_end(const char* p)
{
    bool hadSqBracket = (*p == '['); //an ipv6 address can be specified in an http url only in square brackets
    for (; *p; p++)
    {
        if (*p == '/')
            return p;
        else if (*p == ':')
        {
            if (!hadSqBracket) //not ipv6, must be port then
                return p;
        }
        else if (*p == ']')
        {
            if (hadSqBracket)
                return p; //[host] end, maybe ipv6
            else //invalid char
                return NULL;
        }
    }
    return p; //the terminating null
}
