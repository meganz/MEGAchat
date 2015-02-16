cmake_minimum_required(VERSION 2.8)
project(rtcModule)
include (utils.cmake)

set(optRtcModuleBuildShared 1 CACHE BOOL "Whether to build rtcModule as a shared lib")

set (SRCS
    strophe.jingle.session.cpp
    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
    rtcModule.cpp
    karereCommon.cpp
    lib.cpp
)

add_subdirectory(base)
add_subdirectory(../webrtc-build webrtc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WEBRTC_CXXFLAGS} -std=c++11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WEBRTC_CFLAGS}")

get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)
include_directories(. ${CMAKE_CURRENT_BINARY_DIR} ${STROPHE_INCLUDES} ${WEBRTC_INCLUDES})

add_definitions(-DRTCM_BUILDING ${WEBRTC_DEFS} -fvisibility=hidden)

#-Wno-unused-local-typedefs suppresses a lot of warning form webrtc headers from COMPILE_ASSERT: see https://code.google.com/p/chromium/issues/detail?id=227506
add_definitions(-Wno-unused-local-typedefs)

if(optRtcModuleBuildShared)
    add_library(rtcModule SHARED ${SRCS})
else()
    add_library(rtcModule ${SRCS})
endif()

target_link_targets(rtcModule services)
target_link_libraries(rtcModule webrtc_my pthread)

targetExportDefines(rtcModule "")
targetExportFullpathIncludes(rtcModule ${CMAKE_CURRENT_SOURCE_DIR})
targetExportCompileFlags(rtcModule "")

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
set(CMAKE_DOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
set(CMAKE_DOXYPROJECT_NAME "RTC Module")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
add_custom_target(doc
${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)
