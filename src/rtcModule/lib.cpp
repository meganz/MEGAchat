#include "rtcModule.h"
#include "lib.h"
#include "webrtc/base/ssladapter.h"

using namespace rtcModule;

extern "C"
{
 RTCM_EXPORT IRtcModule* createRtcModule(xmpp_conn_t* conn, IEventHandler* handler,
    ICryptoFunctions* crypto, const char* iceServers)
 {
     try
     {
         artc::init(nullptr);
         return new RtcModule(strophe::Connection(conn), handler, crypto, iceServers);
     }
     catch(std::exception& e)
     {
         KR_LOG_ERROR("Error creating RtcModule instance: %s", e.what());
         return nullptr;
     }
 }
 RTCM_EXPORT int rtcCleanup()
 {
     try
     {
         artc::cleanup();
     }
     catch(std::exception& e)
     {
         KR_LOG_ERROR("RtcMoudle cleanup error: %s", e.what());
         return 1;
     }
     return 0;
 }
}
