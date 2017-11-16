#ifndef URL_H
#define URL_H
#include <string>
#include <stdint.h>

namespace karere
{
class Url
{
protected:
    uint16_t getPortFromProtocol() const;
public:
    std::string protocol;
    std::string host;
    uint16_t port;
    std::string path;
    bool isSecure;
    Url(const std::string& url) { parse(url); }
    Url(): isSecure(false) {}
    bool parse(const std::string& url);
    bool isValid() const { return !host.empty(); }
};
}

#endif
