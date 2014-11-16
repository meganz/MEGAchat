#ifndef _MEGA_BASE_SERVICES_INCLUDED
#define _MEGA_BASE_SERVICES_INCLUDED

namespace mega
{

template <class CB>
static inline void* setTimeout(CB callback, int time)
{
    //TODO: Implement
}
static inline void cancelTimeout(void* handle)
{
    //TODO: implement
}

template <class CB>
static void* setInterval(CB callback, int time)
{
    //TODO: Implement
}
static inline void cancelInterval(void* handle)
{
    //TODO: Implement
}

}
#endif
