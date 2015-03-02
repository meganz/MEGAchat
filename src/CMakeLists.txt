cmake_minimum_required(VERSION 2.8)
project(chatclient)

include (utils.cmake)
include (FindCryptopp.cmake)

set (SRCS
     chatRoom.cpp
     contactList.cpp
     chatClient.cpp
     karereCommon.cpp
     strophe.disco.cpp
     dummyCrypto.cpp
     megaCryptoFunctions.cpp
)

add_definitions(-D_DEBUG -fvisibility=hidden -Wall)
if (CMAKE_COMPILER_ID STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
add_subdirectory(rtcModule)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)

set (optMegaSdkPath "/home/loki/megasdk" CACHE PATH "Mega SDK path")
if (WIN32)
  if (WINPHONE8)
    set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/wp8")
  else()
    set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/win32")
  endif()
else()
  set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/posix")
endif()

set(MEGASDK_INCLUDES "${optMegaSdkPath}/include" "${MEGASDK_PLATFORM_INCLUDES}")
set_property(GLOBAL PROPERTY MEGASDK_INCLUDES ${MEGASDK_INCLUDES})
set(MEGASDK_LIB "${optMegaSdkPath}/src/.libs/libmega.so")

include_directories(
    .
    rtcModule
    base
    ../third-party
    ${CMAKE_CURRENT_BINARY_DIR}
    ${STROPHE_INCLUDES}
    ${MEGASDK_INCLUDES}
)

add_library(chatclient ${SRCS})
target_link_libraries(chatclient
    rtcmodule
    ${MEGASDK_LIB}
    ${CRYPTOPP_LIBRARIES}
)


