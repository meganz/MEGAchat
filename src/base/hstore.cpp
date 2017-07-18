#include <unordered_map>
#include <assert.h>
#include "cservices.h"

extern "C"
{
    
//Handle store
struct HandleItem
{
    unsigned short type;
    void* ptr;
    HandleItem(unsigned short aType, void* aPtr):type(aType), ptr(aPtr){}
    bool operator==(const HandleItem& other) const
    {
        return ((type == other.type) && (ptr == other.ptr));
    }
};

std::unordered_map<megaHandle, HandleItem> gHandleStore;
megaHandle gHandleCtr = 0;

MEGAIO_EXPORT void* services_hstore_get_handle(unsigned short type, megaHandle handle)
{
    auto it = gHandleStore.find(handle);
    if ((it == gHandleStore.end()) || (it->second.type != type))
        return nullptr;
    return it->second.ptr;
}

MEGAIO_EXPORT megaHandle services_hstore_add_handle(unsigned short type, void* ptr)
{
#ifndef NDEBUG
    megaHandle old = gHandleCtr;
#endif
    megaHandle id = ++gHandleCtr;
#ifndef NDEBUG
    if (id < old)
    {
        fprintf(stderr, "ERROR: megaHandle id generator wrapped around\n");
        fflush(stderr);
        abort();
    }
    bool inserted =
#endif
    gHandleStore.emplace(std::piecewise_construct,
                         std::forward_as_tuple(id), std::forward_as_tuple(type, ptr))
#ifndef NDEBUG
    .second
#endif
    ;
    assert(inserted);
    return id;
}

MEGAIO_EXPORT int services_hstore_remove_handle(unsigned short type, megaHandle handle)
{
    auto it = gHandleStore.find(handle);
    if (it == gHandleStore.end())
    {
#ifndef NDEBUG
        fprintf(stderr, "ERROR: services_hstore_remove_handle: Handle not found (id=%u, type=%d)\n", handle, type);
#endif
        return 0;
    }
    if (it->second.type != type)
    {
        fprintf(stderr, "ERROR: services_hstore_remove_handle: Handle found, but requested type %u does not match actual type %u\n", type, it->second.type);
        fflush(stderr);
        return 0;
    }
    gHandleStore.erase(it);
    return 1;
}

}
