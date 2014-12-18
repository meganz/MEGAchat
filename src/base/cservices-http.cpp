#include "cservices.h"
#include <curl/curl.h>
#include <event2/event.h>
#include <assert.h>
#include "gcmpp.h"


#define always_assert(cond) \
    if (!(cond)) SVC_LOG_ERROR("HTTP: Assertion failed: '%s' at file %s, line %d", #cond, __FILE__, __LINE__)

event* gTimerEvent = NULL;
CURLM* gCurlMultiHandle = NULL;
int gNumRunning = 0;

static void le2_onEvent(int fd, short events, void* userp);
static inline void checkCompleted();

void CurlConn_init(CurlConnection* conn, int sockfd)
{
    conn->socket = sockfd;
    conn->curEvents = 0;
    conn->read = event_new(services_get_event_loop(), sockfd, EV_READ, le2_onEvent, conn);
    conn->write = event_new(services_get_event_loop(), sockfd, EV_WRITE, le2_onEvent, conn);
}

static inline void CurlConn_finalize(CurlConnection* conn)
{
    event_del(conn->read);
    event_del(conn->write);
    conn->socket = -1;
    conn->curEvents = 0;
    event_free(conn->read);
    event_free(conn->write);
    conn->read = NULL;
    conn->write = NULL;
}

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

static inline void CurlConn_subscribeToEvents(CurlConnection* conn, int what)
{
    int events = curlToLe2Events(what);
    int changed = events ^ conn->curEvents;
    conn->curEvents = events;
    if (changed & EV_READ) //change requested on read subscription
    {
        if (events & EV_READ)
            event_add(conn->read, nullptr);
        else
            event_del(conn->read);
    }
    if (changed & EV_WRITE)
    {
        if (events & EV_WRITE)
            event_add(conn->write, nullptr);
        else
            event_del(conn->write);
    }
}
static void le2_onEvent(int fd, short events, void* userp)
{
    events = le2ToCurlEvents(events);
    CurlConnection* conn = (CurlConnection*)userp;
    assert(conn);

    int oldNumRunning = gNumRunning;
    int ret = curl_multi_socket_action(gCurlMultiHandle, fd, events, &gNumRunning);
    if (ret != CURLM_OK)
    {
        SVCS_LOG_ERROR("le2_onEvent: curl_multi_socket_action() returned error %d", ret);
        return;
    }
    checkCompleted();
}

static void le2_onTimer(int fd, short kind, void *userp)
{
    int oldNumRunning = gNumRunning;
    int ret = curl_multi_socket_action(gCurlMultiHandle,
        CURL_SOCKET_TIMEOUT, 0, &gNumRunning);
    if (ret != CURLM_OK)
    {
        SVCS_LOG_ERROR("le2_onTimer: curl_multi_socket_action() returned error %d", ret);
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
    CurlConnection* conn = (CurlConnection*)sockp;
//    const char *whatstr[]={ "none", "IN", "OUT", "INOUT", "REMOVE" };
    if (what == CURL_POLL_REMOVE)
    {
        if (conn)
        {
            CurlConn_finalize(conn);
            curl_multi_assign(gCurlMultiHandle, fd, NULL);
        }
    }
    else
    {
        if (!conn)
        {
            int ret = curl_easy_getinfo(e, CURLINFO_PRIVATE, &conn);
            if (!ret || !conn)
            {
                SVCS_LOG_ERROR("curlcb_subscribe_to_events: Assertion failed: Error getting CurlConnection pointer from curl easy handle: %d", ret);
                abort();
            }
            CurlConn_init(conn, fd);
            curl_multi_assign(gCurlMultiHandle, fd, conn);
        }
        CurlConn_subscribeToEvents(conn, what);
    }
    return 0;
}

static int curlcb_subscribe_to_timer(CURLM *multi, long timeout_ms, void* userp)
{
    if (timeout_ms < 0)
        return 0;

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
    gCurlMultiHandle = curl_multi_init();
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_SOCKETFUNCTION, curlcb_subscribe_to_events);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_SOCKETDATA, NULL);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_TIMERFUNCTION, curlcb_subscribe_to_timer);
    curl_multi_setopt(gCurlMultiHandle, CURLMOPT_TIMERDATA, NULL);
    gTimerEvent = evtimer_new(services_get_event_loop(), le2_onTimer, NULL);
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

static const char* url_find_host_end(const char* p);
t_string_bounds services_http_url_get_host(const char* url)
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
