# At this stage, this file is intended for building MEGAChat library directly on windows with Visual Studio (just for chat so far), and/or exploring the chat API with example app MEGAclc
# Build chat and the SDK with CURL and OPENSSL

cmake_minimum_required(VERSION 2.8)
project(karere)

if (WIN32)
    if("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
        SET(build_64_bit 1)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        SET(build_64_bit 0)
    endif()
else()
    set (build_64_bit 1 CACHE TYPE BOOL)
endif()

# create these cache variables first, these must be on for MEGAchat.  The default in the SDK is off, but it will see they are already on in the CMakeCache.txt
set (USE_SODIUM 1 CACHE TYPE BOOL)
set (ENABLE_CHAT 1 CACHE TYPE BOOL)
set (USE_WEBRTC 1 CACHE TYPE BOOL)

set (UNCHECKED_ITERATORS 0 CACHE TYPE BOOL)  # to use libwebrtc on windows with checked iterators turned off in the debug VC++ runtime, modify your stl headers first to disable that, and then build it

#specify where 3rd party libraries are available
set(Mega3rdPartyDir "${CMAKE_CURRENT_LIST_DIR}/../../../3rdParty_chat"  CACHE TYPE STRING)
set(KarereDir "${CMAKE_CURRENT_LIST_DIR}/../..")

message("CONFIG: ${CONFIG}")

IF (build_64_bit)
    set(WebRtcDir "${Mega3rdPartyDir}/libwebrtc/build64debug") 
ELSE()
    set(WebRtcDir "${Mega3rdPartyDir}/libwebrtc/build32debug")    #todo: figure out if we can have debug/release at the same path
ENDIF()

IF (USE_WEBRTC)
   set(KARERE_DISABLE_WEBRTC 0)
ELSE()
   set(KARERE_DISABLE_WEBRTC 1)
ENDIF()

# this line points to the MEGA SDK repo that you want to build MEGAChat against
include(${CMAKE_CURRENT_LIST_DIR}/../../third-party/mega/contrib/cmake/CMakeLists.txt)

if (NOT ENABLE_CHAT OR NOT USE_OPENSSL)
    message(FATAL_ERROR "ENABLE_CHAT must be enabled in the SDK build, also USE_OPENSSL as libwebsockets requires it")
endif()


if (WIN32)
    ImportVcpkgLibrary(websockets         "${Mega3rdPartyDir}/libwebsockets/build32debug/include" "${Mega3rdPartyDir}/libwebsockets/build32debug/lib/Debug/websockets_static.lib" "${Mega3rdPartyDir}/build32release/lib/websockets.lib")
    ImportVcpkgLibrary(libuv              "${vcpkg_dir}/include" "${vcpkg_dir}/debug/lib/libuv.lib" "${vcpkg_dir}/lib/libuv.lib")

    if (USE_WEBRTC)
        ImportVcpkgLibrary(webrtc         "${WebRtcDir}/include" "${WebRtcDir}/lib/webrtc.lib" "${WebRtcDir}/lib/webrtc.lib")
        ImportVcpkgLibrary(webrtc2        "${WebRtcDir}/include/webrtc" "${WebRtcDir}/lib/webrtc.lib" "${WebRtcDir}/lib/webrtc.lib")
        ImportVcpkgLibrary(webrtcDeps     "${WebRtcDir}/webrtc/src/third_party/libyuv/include" "${WebRtcDir}/lib/webrtc.lib" "${WebRtcDir}/lib/webrtc.lib")
        add_definitions( -DWEBRTC_WIN )
    endif()
else (WIN32)
    add_definitions( -DWEBRTC_POSIX )
endif(WIN32)

add_definitions( -DUSE_LIBWEBSOCKETS )

ImportHeaderLibrary(rapidjson    "${Mega3rdPartyDir}/rapidjson/include")
 
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
target_compile_definitions(karere PUBLIC MEGA_FULL_STATIC $<${KARERE_DISABLE_WEBRTC}:KARERE_DISABLE_WEBRTC> ) 
target_link_libraries(karere PUBLIC Mega sqlite3 rapidjson websockets libuv $<${USE_WEBRTC}:webrtc> $<${USE_WEBRTC}:webrtc2> $<${USE_WEBRTC}:webrtcDeps> )
if (WIN32)
target_link_libraries(karere PUBLIC Iphlpapi.lib Psapi.lib Userenv.lib Msdmo.lib Strmiids.lib Dmoguids.lib Winmm.dll wmcodecdspuuid.lib Wldap32.lib)
endif()

add_executable(megaclc ${KarereDir}/examples/megaclc/megaclc.cpp)
target_link_libraries(megaclc PUBLIC karere)
target_compile_definitions(megaclc PRIVATE MEGA_FULL_STATIC $<${KARERE_DISABLE_WEBRTC}:KARERE_DISABLE_WEBRTC>)
if (NOT NO_READLINE)
    target_link_libraries(megaclc PUBLIC readline dl pthread)
endif (NOT NO_READLINE)

