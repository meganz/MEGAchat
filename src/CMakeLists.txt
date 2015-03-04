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
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
endif()

add_subdirectory(rtcModule)
get_property(SERVICES_INCLUDES GLOBAL PROPERTY SERVICES_INCLUDES)

set (optMegaSdkPath "~/megasdk" CACHE PATH "Mega SDK path")
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
set(MEGASDK_LIB "${optMegaSdkPath}/src/.libs/libmega${CMAKE_SHARED_LIBRARY_SUFFIX}")

include_directories(
    .
    rtcModule
    base
    ../third-party
    ${CMAKE_CURRENT_BINARY_DIR}
    ${MEGASDK_INCLUDES}
    ${SERVICES_INCLUDES}
    ${CRYPTOPP_INCLUDE_DIRS}
)

add_library(chatclient ${SRCS})
target_link_libraries(chatclient
    rtcmodule
    ${MEGASDK_LIB}
    ${CRYPTOPP_LIBRARIES}
)


