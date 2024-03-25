#ifndef __RTCM_PRIVATE
#define __RTCM_PRIVATE

#define RTCM_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_ERROR(fmtString, ...) KARERE_LOG_ERROR(krLogChannel_rtc, fmtString, ##__VA_ARGS__); \
    char logLine[300]; \
    snprintf(logLine, 300, fmtString, ##__VA_ARGS__); \
mMegaApi.call(&::mega::MegaApi::sendChatLogs, logLine, getOwnPeerId(), mCallid, CHATLOGS_PORT); \

#define RTCM_LOG_EVENT(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtcevent, fmtString, ##__VA_ARGS__)

#endif
