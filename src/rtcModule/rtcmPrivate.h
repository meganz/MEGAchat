#ifndef __RTCM_PRIVATE
#define __RTCM_PRIVATE

#define RTCM_EVENT(call, name, ...)                       \
    KR_LOG_RTC_EVENT("%s->" #name, call->mSid.c_str());    \
    call->mHandler->name(__VA_ARGS__)

#define RTCM_GLOBAL_LEVENT(handler, name, ...)    \
   KR_LOG_RTC_EVENT("GlobalEvent: %s", #name);    \
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
