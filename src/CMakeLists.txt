cmake_minimum_required(VERSION 2.8)
project(rtcModule)
include (utils.cmake)
set (optWebrtcCmakeBuild "../../webrtc-buildsys" CACHE PATH "Path to CMakeLists.txt file that puts together webrtc libs")
set (optStrophePath "../../strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")
set (SRCS
    strophe.jingle.session.cpp
    strophe.disco.cpp
    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
    DummyCrypto.cpp
    rtcModule.cpp
    lib.cpp
)

list(APPEND CMAKE_CXX_FLAGS -std=c++11)
add_subdirectory("${optWebrtcCmakeBuild}" webrtc)
add_subdirectory("${optStrophePath}" strophe)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${optStrophePath} ${optStrophePath}/include)
add_definitions(-DRTCM_BUILDING -fvisibility=hidden)
add_library(rtcModule SHARED ${SRCS})
target_link_targets(rtcModule webrtc_my)
target_link_libraries(rtcModule strophe pthread)

