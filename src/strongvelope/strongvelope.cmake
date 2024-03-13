set(CHATLIB_STRV_HEADERS
    strongvelope/cryptofunctions.h
    strongvelope/strongvelope.h
    strongvelope/tlvstore.h
)

set(CHATLIB_STRV_SOURCES
    strongvelope/strongvelope.cpp
)

target_sources(CHATlib
    PRIVATE
    ${CHATLIB_STRV_HEADERS}
    ${CHATLIB_STRV_SOURCES}
)

target_include_directories(CHATlib PUBLIC ${CMAKE_CURRENT_LIST_DIR})
