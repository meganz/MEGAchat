#include "karereCommon.h"
#include "stringUtils.h"
#include "sdkApi.h"
#include "base/timers.hpp"
#include "megachatapi_impl.h"
#include "waiter/libuvWaiter.h"

#ifndef KARERE_DISABLE_WEBRTC
namespace rtcModule {void globalCleanup(); }
#endif

namespace karere
{
const char* gDbSchemaVersionSuffix = "5";
// 2 --> +3: invalidate cached chats to reload history (so call-history msgs are fetched)
// 3 --> +4: invalidate both caches, SDK + MEGAchat, if there's at least one chat (so deleted chats are re-fetched from API)
// 4 --> +5: modify attachment, revoke, contact and containsMeta and create a new table node_history

bool gCatchException = true;

void globalInit(void(*postFunc)(void*, void*), uint32_t options, const char* logPath, size_t logSize)
{
    if (logPath)
    {
        karere::gLogger.logToFile(logPath, logSize);
    }
    services_init(postFunc, options);
}

void globalCleanup()
{
#ifndef KARERE_DISABLE_WEBRTC
    rtcModule::globalCleanup();
#endif
    services_shutdown();
}

void RemoteLogger::log(krLogLevel /*level*/, const char* msg, size_t len, unsigned /*flags*/)
{
//WARNING:
//This is a logger callback, and can be called by worker threads.
//Also, we must not log from within this callback, because that will cause re-entrancy
//in the logger.
//Therefore, we must copy the message and return asap, without doing anything that may
//log a message.

    if (!msg)
        return;
    auto json = std::make_shared<std::string>("{\"msg\":\"");
    const char* start = strchr(msg, ']');
    if (!start)
        start = msg;
    else
        start++; //skip the closing bracket
    json->append(replaceOccurrences(std::string(start, len-(start-msg+1)), "\"", "\\\"")).append("\"}");
    *json = replaceOccurrences(*json, "\n", "\\n");
    *json = replaceOccurrences(*json, "\t", "\\t");
    std::string *aid = &mAid;
    mApi.call(&::mega::MegaApi::sendChatLogs, json->c_str(), aid->c_str())
        .fail([](const promise::Error& err)
        {
            if (err.type() == ERRTYPE_MEGASDK)
            {
                KR_LOG_WARNING("Error %d logging error message to remote server:\n%s",
                    err.code(), err.what());
            }
            return err;
        });
}

void init_uv_timer(void *ctx, uv_timer_t *timer)
{
    uv_timer_init(((::mega::LibuvWaiter *)(((megachat::MegaChatApiImpl *)ctx)->waiter))->eventloop, timer);
}
}
