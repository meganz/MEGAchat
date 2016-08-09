#include "karereCommon.h"
#include <services-http.hpp>
#include "chatClient.h"
#include "stringUtils.h"

namespace karere
{
class Client;
//std::unique_ptr<Client> gClient;

void RemoteLogger::log(krLogLevel level, const char* msg, size_t len, unsigned flags)
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
    marshallCall([json, level]()
    {
        http::postString("https://stats.karere.mega.nz/msglog?aid=kn-asdasdsdf&t=e",
            json, "application/json")
        .fail([](const promise::Error& err)
        {
            KR_LOG_WARNING("Error %d logging error message to remote server:\n%s",
                err.code(), err.what());
            return err;
        });
    });
}
}
