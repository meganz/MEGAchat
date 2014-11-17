#ifndef RTCM_SOLIB_H
#define RTCM_SOLIB_H

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

#endif // LIB_H
