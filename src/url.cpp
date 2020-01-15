#include "url.h"
#include <algorithm>
#include <stdexcept>

namespace karere
{
void Url::parse(const std::string& url)
{
    if (url.empty())
        throw std::runtime_error("Url::Parse: Url is empty");
    protocol.clear();
    port = 0;
    host.clear();
    path.clear();
    size_t ss = url.find("://");
    if (ss != std::string::npos)
    {
        protocol = url.substr(0, ss);
        std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);
        ss += 3;
    }
    else
    {
        ss = 0;
        protocol = "http";
    }
    char last = protocol[protocol.size()-1];
    isSecure = (last == 's');

    size_t i = ss;
    for (; i<url.size(); i++)
    {
        char ch = url[i];
        if (ch == ':') //we have port
        {
            size_t ps = i+1;
            host = url.substr(ss, i-ss);
            for (; i<url.size(); i++)
            {
                ch = url[i];
                if ((ch == '/') || (ch == '?'))
                {
                    break;
                }
            }
            port = std::stol(url.substr(ps, i-ps));
            break;
        }
        else if ((ch == '/') || (ch == '?'))
            break;
    }

    if (host.empty())
        host = url.substr(ss, i-ss);

    if (i < url.size()) //not only host and protocol
    {
        //i now points to '/' or '?' and host and port must have been set
        path = (url[i] == '/') ? url.substr(i+1) : url.substr(i); //ignore the leading '/'
    }
    if (!port)
    {
        port = getPortFromProtocol();
    }
    if (host.empty())
        throw std::runtime_error("Url::parse: Invalid URL '"+url+"', host is empty");
}

uint16_t Url::getPortFromProtocol() const
{
    if ((protocol == "http") || (protocol == "ws"))
        return 80;
    else if ((protocol == "https") || (protocol == "wss"))
        return 443;
    else
        return 0;
}
}
