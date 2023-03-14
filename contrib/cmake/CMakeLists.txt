# At this stage, this file is intended for building MEGAChat library directly on windows with Visual Studio (just for chat so far), and/or exploring the chat API with example app MEGAclc
# Build chat and the SDK with CURL and OPENSSL

cmake_minimum_required(VERSION 3.15)
project(karere)

# Qt settings
if (USE_QT)
    set (MEGA_QT_REQUIRED_COMPONENTS Core Gui)
    set (MEGA_QT_LINK_LIBRARIES Qt5::Core Qt5::Gui)
endif()

if (WIN32)
    if("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
        SET(build_64_bit 1)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        SET(build_64_bit 0)
    endif()
else()
    set (build_64_bit 1 CACHE BOOL "True if building for 64bit.")
endif()

# create these cache variables first, these must be on for MEGAchat.  The default in the SDK is off, but it will see they are already on in the CMakeCache.txt
set (USE_SODIUM 1 CACHE STRING "Sodium is a requirement for MEGAchat, turn on in the MEGA SDK")
set (ENABLE_CHAT 1 CACHE STRING "Turn on ENBABLE_CHAT in the MEGA SDK")
set (USE_WEBRTC 1 CACHE STRING "Turn on WEBRTC to support voice and/or video calls, turn off for much simpler dependencies")
set (USE_LIBUV 1 CACHE STRING "Libwebsocketes needs libuv")

if (WIN32)
set (UNCHECKED_ITERATORS 0 CACHE STRING "Turn off UNCHECKED_ITERATORS if your third party dependencies were built without them (only relevant for DEBUG)")  # to use libwebrtc on windows with checked iterators turned off in the debug VC++ runtime, modify your stl headers first to disable that, and then build it
endif()

#specify where 3rd party libraries are available
set(Mega3rdPartyDir "${CMAKE_CURRENT_LIST_DIR}/../../third-party"  CACHE STRING "Controllable third party library location")
set(KarereDir "${CMAKE_CURRENT_LIST_DIR}/../..")

#and where the SDK is.  For buliding megachat alone, default to the old location
set(MegaSdkDir "${CMAKE_CURRENT_LIST_DIR}/../../third-party/mega"  CACHE STRING "Controllable MEGA SDK library location")

message("CONFIG: ${CONFIG}")

IF (build_64_bit)
    set(WebRtcDir "${Mega3rdPartyDir}/libwebrtc/build64debug")
ELSE()
    set(WebRtcDir "${Mega3rdPartyDir}/libwebrtc/build32debug")    #todo: figure out if we can have debug/release at the same path
ENDIF()

# this line points to the MEGA SDK repo that you want to build MEGAChat against
include(${MegaSdkDir}/contrib/cmake/CMakeLists.txt)

if (NOT ENABLE_CHAT OR NOT USE_OPENSSL)
    message(FATAL_ERROR "ENABLE_CHAT must be enabled in the SDK build, also USE_OPENSSL as libwebsockets requires it")
endif()

# Import websockets.
ImportStdVcpkgLibrary(websockets websockets websockets libwebsockets libwebsockets)

if (WIN32)
    if (USE_WEBRTC)
        # additional directories used in this project, after the main one incluced by the MEGA SDK
        include_directories( "${WebRtcDir}/include/webrtc" )
        include_directories( "${WebRtcDir}/webrtc/src/third_party/libyuv/include" )
    endif()
else(WIN32)
    include_directories("${Mega3rdPartyDir}/include")
    link_directories("${Mega3rdPartyDir}/lib")
endif(WIN32)

add_definitions( -DUSE_LIBWEBSOCKETS )

ImportHeaderLibrary(rapidjson    "${KarereDir}/third-party")

set (SRCS
    ${KarereDir}/src/karereCommon.cpp
    ${KarereDir}/src/base64url.cpp
    ${KarereDir}/src/chatClient.cpp
    ${KarereDir}/src/userAttrCache.cpp
    ${KarereDir}/src/url.cpp
    ${KarereDir}/src/chatd.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    ${KarereDir}/src/strongvelope/strongvelope.cpp
    ${KarereDir}/src/presenced.cpp
    ${KarereDir}/src/megachatapi.cpp
    ${KarereDir}/src/megachatapi_impl.cpp
    ${KarereDir}/src/kareredb.cpp
    ${KarereDir}/src/chatclientDb.cpp

    ${KarereDir}/src/base/logger.cpp
    ${KarereDir}/src/net/websocketsIO.cpp
    ${KarereDir}/src/net/libwebsocketsIO.cpp
    ${KarereDir}/src/waiter/libuvWaiter.cpp

    $<${USE_WEBRTC}:${KarereDir}/src/rtcModule/webrtc.cpp>
    $<${USE_WEBRTC}:${KarereDir}/src/rtcModule/webrtcAdapter.cpp>
    $<${USE_WEBRTC}:${KarereDir}/src/rtcModule/rtcStats.cpp>
    $<${USE_WEBRTC}:${KarereDir}/src/rtcCrypto.cpp>
)

set(KARERE_DEFINES -DHAVE_KARERE_LOGGER ${LIBMEGA_DEFINES})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${KarereDir}/src -P ${KarereDir}/src/genDbSchema.cmake
    DEPENDS ${KarereDir}/src/dbSchema.sql ${KarereDir}/src/genDbSchema.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(karere ${SRCS} ${KarereDir}/src/base/cservices.cpp )
target_include_directories(karere PRIVATE ${KarereDir}/src/base rapidjson)
target_include_directories(karere PUBLIC ${KarereDir}/src )
target_include_directories(karere PUBLIC $<${USE_WEBRTC}:${KarereDir}/src/rtcModule>  )
target_compile_definitions(karere PUBLIC MEGA_FULL_STATIC $<$<NOT:${USE_WEBRTC}>:KARERE_DISABLE_WEBRTC> )
target_link_libraries(karere PUBLIC Mega rapidjson websockets uv $<${USE_WEBRTC}:webrtc> )
target_link_libraries(websockets INTERFACE cap) # websockets depends on cap
if (WIN32)
target_link_libraries(karere PUBLIC Iphlpapi.lib Psapi.lib Userenv.lib Msdmo.lib Strmiids.lib Dmoguids.lib Winmm.dll wmcodecdspuuid.lib Wldap32.lib)
endif()

#test apps
add_executable(sdk_test ${KarereDir}/tests/sdk_test/sdk_test.cpp)
target_link_libraries(sdk_test PUBLIC karere)
target_compile_definitions(sdk_test PRIVATE MEGA_FULL_STATIC $<$<NOT:${USE_WEBRTC}>:KARERE_DISABLE_WEBRTC>)

add_executable(megaclc ${KarereDir}/examples/megaclc/megaclc.cpp)
target_link_libraries(megaclc PUBLIC karere)
target_compile_definitions(megaclc PRIVATE MEGA_FULL_STATIC $<$<NOT:${USE_WEBRTC}>:KARERE_DISABLE_WEBRTC>)
if (NOT NO_READLINE)
    target_link_libraries(megaclc PUBLIC readline dl pthread)
endif (NOT NO_READLINE)

if (WIN32)
    set_property(TARGET karere PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<${MEGA_LINK_DYNAMIC_CRT}:DLL>")
    set_property(TARGET megaclc PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<${MEGA_LINK_DYNAMIC_CRT}:DLL>")
    set_property(TARGET sdk_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<${MEGA_LINK_DYNAMIC_CRT}:DLL>")

    target_compile_options(karere PRIVATE /WX-)  # tackling the sheer number of warnings in this project is a big job

endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17") 
