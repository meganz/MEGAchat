#ifndef SVRLIST_PROVIDER_FORWARDS
#define SVRLIST_PROVIDER_FORWARDS
#include <vector>
#include <memory>

namespace karere
{
struct HostPortServerInfo;
struct TurnServerInfo;

template <class S>
using ServerList = std::vector<std::shared_ptr<S> >;

template <class>
class ServerProvider;

template <class>
class GelbProvider;

template <class>
class StaticProvider;

template <class>
class FallbackServerProvider;
}
#endif
