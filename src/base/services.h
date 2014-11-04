#ifndef _MEGA_BASE_SERVICES_INCLUDED
#define _MEGA_BASE_SERVICES_INCLUDED

namespace mega
{

template <class CB>
void* setTimeout(CB callback, int time)
{
    //TODO: Implement
}
void cancelTimeout(void* handle)
{
    //TODO: implement
}

template <class CB>
void* setInterval(CB callback, int time)
{
    //TODO: Implement
}
void cancelInterval(void* handle)
{
    //TODO: Implement
}

}
#endif
