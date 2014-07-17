cmake_minimum_required(VERSION 2.8)
project(karere)
include (utils.cmake)

set(SRCS
	strophe.jingle.session.cpp
#    strophe.jingle.cpp
	webrtcAdapter.cpp
#    strophe.jingle.sdp.cpp
#    rtcSession.cpp
#  	test-promise.cpp
)


add_definitions(-std=c++11)
add_subdirectory(../../webrtc-build/mybuild webrtc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(karere ${SRCS})
target_link_targets(karere webrtc_my)
target_link_libraries(karere)

