cmake_minimum_required(VERSION 2.8)
project(rtcModule)
include (utils.cmake)
set (optWebrtcCmakeBuild "../../webrtc-buildsys" CACHE PATH "Path to CMakeLists.txt file that puts together webrtc libs")
set (optStrophePath "../../strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")
set (SRCS
    strophe.jingle.session.cpp
    strophe.jingle.cpp
    webrtcAdapter.cpp
    strophe.jingle.sdp.cpp
    rtcModule.cpp
    base/guiCallMarshaller.c
    lib.cpp
)

list(APPEND CMAKE_CXX_FLAGS -std=c++11)
add_subdirectory("${optWebrtcCmakeBuild}" webrtc)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${optStrophePath} ${optStrophePath}/include)
add_definitions(-DRTCM_BUILDING -fvisibility=hidden)

# The below line should be uncommented if dlls can't link to dynamic symbols exported
# by the main app executable
#
# add_definitions(-DMEGA_GCM_CLIENT_INIT_API=rtcModule)

add_library(rtcModule SHARED ${SRCS})
target_link_targets(rtcModule webrtc_my)
target_link_libraries(rtcModule ${optStrophePath}/build/libstrophe.so pthread)

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
