# At this stage, this file is intended for building MEGAChat library directly on windows with Visual Studio (just for chat so far), and/or exploring the chat API with example app MEGAclc
# Build chat and the SDK with CURL and OPENSSL

cmake_minimum_required(VERSION 2.8)
project(karere)

#specify where 3rd party libraries are available
set(Mega3rdPartyDir "${CMAKE_CURRENT_LIST_DIR}/../../../3rdparty")

# this line points to the MEGA SDK repo that you want to build MEGAChat against
include(${CMAKE_CURRENT_LIST_DIR}/../../../sdk/contrib/cmake/CMakeLists.txt)

set(KarereDir "${CMAKE_CURRENT_LIST_DIR}/../..")

ImportVcpkgLibrary(websockets         "${vcpkg_dir}/include" "${vcpkg_dir}/debug/lib/websockets.lib" "${vcpkg_dir}/lib/websockets.lib")
ImportVcpkgLibrary(uv         "${vcpkg_dir}/include" "${vcpkg_dir}/debug/lib/libuv.lib" "${vcpkg_dir}/lib/libuv.lib")

add_definitions( -DUSE_LIBWEBSOCKETS -DLWS_WITH_LIBUV )

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
)

set(KARERE_DEFINES -DHAVE_KARERE_LOGGER ${LIBMEGA_DEFINES})

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${KarereDir}/src -P ${KarereDir}/src/genDbSchema.cmake
    DEPENDS ${KarereDir}/src/dbSchema.sql ${KarereDir}/src/genDbSchema.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(karere ${SRCS} ${KarereDir}/src/base/cservices.cpp )
target_include_directories(karere PRIVATE ${KarereDir}/src/base)
target_include_directories(karere PUBLIC ${KarereDir}/src )
target_compile_definitions(karere PUBLIC MEGA_FULL_STATIC KARERE_DISABLE_WEBRTC ) 
target_link_libraries(karere PUBLIC Mega sqlite3 rapidjson ssl sslcrypto websockets uv )
if (WIN32)
target_link_libraries(karere PUBLIC Iphlpapi.lib Psapi.lib Userenv.lib)
endif()

add_executable(megaclc ${KarereDir}/examples/megaclc/megaclc.cpp)
target_link_libraries(megaclc PUBLIC karere)
target_compile_definitions(megaclc PRIVATE MEGA_FULL_STATIC KARERE_DISABLE_WEBRTC)
if (NOT NO_READLINE)
    target_link_libraries(megaclc PUBLIC readline)
endif (NOT NO_READLINE)

