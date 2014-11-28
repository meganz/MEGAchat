cmake_minimum_required(VERSION 2.8)
project(rtcModule)
include (utils.cmake)

set (optWebrtcCmakeBuild "../../webrtc-buildsys" CACHE PATH "Path to CMakeLists.txt file that puts together webrtc libs")
set (SRCS
    strophe.jingle.session.cpp
    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
    rtcModule.cpp
    base/guiCallMarshaller.c
    karereCommon.cpp
    lib.cpp
)

add_subdirectory(base)
add_subdirectory("${optWebrtcCmakeBuild}" webrtc)

list(APPEND CMAKE_CXX_FLAGS -std=c++11)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${STROPHE_INCLUDES})
add_definitions(-DRTCM_BUILDING -fvisibility=hidden)

# The below line should be uncommented if dlls can't link to dynamic symbols exported
# by the main app executable
#
# add_definitions(-DMEGA_GCM_CLIENT_INIT_API=rtcModule)

if(optRtcModuleBuildShared)
    add_library(rtcModule SHARED ${SRCS})
else()
    add_library(rtcModule ${SRCS})
endif()

target_link_targets(rtcModule webrtc_my services)
target_link_libraries(rtcModule pthread)

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
