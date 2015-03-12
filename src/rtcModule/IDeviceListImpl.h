#ifndef IDEVICELISTIMPL_H
#define IDEVICELISTIMPL_H
//This header is separated from ITypesImpl because it accesses webrtc-internal headers,
//and should be included only by webrtc module, and not by the user. However, the user needs
//to include ItypesImpl.h in order to implement the ICryptoFunctions interface.
#include "ITypes.h"
#include <webrtc/base/common.h>

namespace rtcModule
{
class IDeviceListImpl: public IDeviceList
{
protected:
    const artc::DeviceList& mDevices;
public:
    IDeviceListImpl(const artc::DeviceList& devices): mDevices(devices){}
    virtual size_t size() const {return mDevices.size();}
    virtual bool empty() const {return mDevices.empty();}
    virtual CString name(size_t idx) const {return mDevices[idx].name;}
    virtual CString id(size_t idx) const {return mDevices[idx].id;}
};
}
#endif // IDEVICELISTIMPL_H
