
set(CHATLIB_WAITER_HEADERS
    waiter/libuvWaiter.h
)

set(CHATLIB_WAITER_SOURCES
    waiter/libuvWaiter.cpp
)

target_sources(CHATlib
    PRIVATE
    ${CHATLIB_WAITER_HEADERS}
    ${CHATLIB_WAITER_SOURCES}
)
