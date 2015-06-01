cmake_minimum_required(VERSION 2.8)
project(rtcModule)
include (../utils.cmake)

set(optRtcModuleBuildShared 1 CACHE BOOL "Whether to build rtcModule as a shared lib")

set (SRCS
    strophe.jingle.session.cpp
    strophe.jingle.cpp
    strophe.jingle.sdp.cpp
    webrtcAdapter.cpp
    rtcStats.cpp
    rtcModule.cpp
    lib.cpp
    ../karereCommon.cpp
)

add_subdirectory(../base base)
add_subdirectory(../../webrtc-build webrtc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WEBRTC_CXXFLAGS} -std=c++11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WEBRTC_CFLAGS}")

include_directories(. .. ../base ${CMAKE_CURRENT_BINARY_DIR}
    ${SERVICES_INCLUDE_DIRS} ${WEBRTC_INCLUDES})

set(RTCMODULE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "rtcModule include dirs for dependent projects")

add_definitions(-DRTCM_BUILDING ${WEBRTC_DEFS} -fvisibility=hidden)

#-Wno-unused-local-typedefs suppresses a lot of warning form webrtc headers from COMPILE_ASSERT: see https://code.google.com/p/chromium/issues/detail?id=227506
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

if (NOT ANDROID AND NOT WIN32)
set(SYSLIBS pthread)
endif()

if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    list(APPEND SYSLIBS ${CLANG_STDLIB})
endif()

if(optRtcModuleBuildShared)
    add_library(rtcmodule SHARED ${SRCS})
else()
    add_library(rtcmodule ${SRCS})
endif()

target_link_libraries(rtcmodule services webrtc_my ${SYSLIBS})

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(CMAKE_DOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
    set(CMAKE_DOXYPROJECT_NAME "RTC Module")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(rtc-doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)
