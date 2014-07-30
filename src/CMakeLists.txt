cmake_minimum_required(VERSION 2.8)
project(karere)
include (utils.cmake)

set(optStrophePath "/home/dbserver/libstrophe-my" CACHE PATH "path to Strophe")
set(SRCS
    strophe.jingle.session.cpp
#    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
#    rtcSession.cpp
#  	test-promise.cpp
)


list(APPEND CMAKE_CXX_FLAGS -std=c++11)
add_subdirectory(../../webrtc-build/mybuild webrtc)
add_subdirectory(${optStrophePath} strophe)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${optStrophePath})

add_executable(karere ${SRCS})
target_link_targets(karere webrtc_my)
target_link_libraries(karere)

