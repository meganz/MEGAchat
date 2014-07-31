cmake_minimum_required(VERSION 2.8)
project(karere)
include (utils.cmake)

set(optWebrtcPath "../../webrtc-build/mybuild" CACHE PATH "Path to webrtc, with the custom cmake build script")
set(optStrophePath "/home/dbserver/libstrophe-my" CACHE PATH "path to Strophe")
set(SRCS
    strophe.jingle.session.cpp
#    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
    karereCommon.cpp
#    rtcSession.cpp
#  	test-promise.cpp
)


list(APPEND CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-unused-local-typedefs")
add_subdirectory(${optWebrtcPath} webrtc)
add_subdirectory(${optStrophePath} strophe)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${optStrophePath})

add_executable(karere ${SRCS})
target_link_targets(karere webrtc_my)
target_link_libraries(karere winmm)

