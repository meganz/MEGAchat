#include <services-dns.hpp>
namespace mega
{
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__ ((visibility ("default")))
#endif
DnsCache gDnsCache;
}
