#ifndef RTCM_SOLIB_H
#define RTCM_SOLIB_H

#include "IRtcModule.h"
#include "ICryptoFunctions.h"

#ifdef _WIN32
    #define RTCM_EXPORT __declspec(dllexport)
    #define RTCM_IMPORT __declspec(dllimport)
#else
    #define RTCM_EXPORT __attribute__ ((visibility ("default")))
    #define RTCM_IMPORT
#endif

#ifdef RTCM_BUILDING
  #define RTCM_IMPEXP RTCM_EXPORT
#else
  #define RTCM_IMPEXP RTCM_IMPORT
#endif
extern "C"
{
RTCM_IMPEXP rtcModule::IRtcModule*
    createRtcModule(xmpp_conn_t* conn, rtcModule::IEventHandler* handler,
        rtcModule::ICryptoFunctions* crypto,
        const char* iceServers);
RTCM_IMPEXP int rtcCleanup();

}
#endif // LIB_H
