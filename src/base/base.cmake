
set(CHATLIB_BASE_HEADERS
    base/addrinfo.hpp
    base/asyncTools.h
    base/cservices.h
    base/cservices-thread.h
    base/gcm.h
    base/gcmpp.h
    base/loggerChannelConfig.h
    base/loggerConsole.h
    base/loggerFile.h
    base/logger.h
    base/promise.h
    base/retryHandler.h
    base/services.h
    base/timers.hpp
    base/trackDelete.h
)

set(CHATLIB_BASE_SOURCES
    base/logger.cpp
    base/cservices.cpp
)

target_sources(CHATlib
    PRIVATE
    ${CHATLIB_BASE_HEADERS}
    ${CHATLIB_BASE_SOURCES}
)

target_include_directories(CHATlib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
