
set(CHATLIB_NET_HEADERS
    net/libwebsocketsIO.h
    net/websocketsIO.h
)

set(CHATLIB_NET_SOURCES
    net/libwebsocketsIO.cpp
    net/websocketsIO.cpp
)

target_sources(CHATlib
    PRIVATE
    ${CHATLIB_NET_HEADERS}
    ${CHATLIB_NET_SOURCES}
)
