#ifndef __RTCM_PRIVATE
#define __RTCM_PRIVATE

#define RTCM_LOG_DEBUG(fmtString,...) KARERE_LOG_DEBUG(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_INFO(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_WARNING(fmtString,...) KARERE_LOG_WARNING(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_ERROR(fmtString,...) KARERE_LOG_ERROR(krLogChannel_rtc, fmtString, ##__VA_ARGS__)
#define RTCM_LOG_EVENT(fmtString,...) KARERE_LOG_INFO(krLogChannel_rtcevent, fmtString, ##__VA_ARGS__)

#define RTCM_EVENT(call, name, ...)                       \
    RTCM_LOG_EVENT("%s->" #name, call->mSid.c_str());   \
    call->mHandler->name(__VA_ARGS__)

#define RTCM_GLOBAL_EVENT(handler, name, ...)    \
   RTCM_LOG_EVENT("GlobalEvent: %s", #name);    \
    handler->event(__VA_ARGS__)
#define GET_CALL(sid, expectedState, whatIfNot)   \
    auto it = find(sid);                          \
    if (it == end()) {                            \
        whatIfNot;                                \
    }                                             \
    auto& call = it->second;                      \
    if (call->state() != expectedState) {         \
    whatIfNot;                                    \
    }

#endif
