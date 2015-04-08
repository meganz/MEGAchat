cmake_minimum_required(VERSION 2.8)
project(chatclient)

include (utils.cmake)
include (FindCryptopp.cmake)
find_package(OpenSSL REQUIRED)

set (SRCS
    ../third-party/murmurHash/MurmurHash3.cpp
    karereCommon.cpp
    strophe.disco.cpp
    dummyCrypto.cpp
    megaCryptoFunctions.cpp
    chatClient.cpp
    contactList.cpp
    ./text_filter/text_filter.cpp
    ./text_filter/default_link_handler.cpp
    ./text_filter/default_emoticon_handler.cpp
)

add_definitions(-D_DEBUG -fvisibility=hidden -Wall)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
endif()

add_subdirectory(rtcModule)
get_property(SERVICES_INCLUDES GLOBAL PROPERTY SERVICES_INCLUDES)

#find Mega SDK and its dependencies
find_path(MEGASDK_PUBLIC_INCLUDE_DIR megaapi.h)
find_library(MEGASDK_LIBS NAMES libmega)
if (NOT MEGASDK_PUBLIC_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find mega sdk header megaapi.h")
endif()
if (NOT MEGASDK_LIB)
    message(FATAL_ERROR "Could not find mega sdk lib (but header found)")
endif()

if (NOT WIN32)
    find_library(CARES_LIB libcares)
    if (NOT CARES_LIB)
        message(FATAL_ERROR "Could not find c-ares library, neede by Mega SDK")
    endif()
    #c-ares is the only megasdk dependency that we don't already include,
    #so add it to the megasdk libs
    list(APPEND MEGASDK_LIBS ${CARES_LIB})
endif()
#add mega platform includes
find_path(
    MEGASDK_PLATFORM_INCLUDES
    NAMES megawaiter.h meganet.h
    PATHS "${MEGASDK_PUBLIC_INCLUDE_DIR}/mega"
    PATH_SUFFIXES "wp8 win32 posix"
)
set(MEGASDK_INCLUDES "${MEGASDK_PUBLIC_INCLUDE_DIR}" "${MEGASDK_PUBLIC_INCLUDE_DIR}/mega" "${MEGASDK_PLATFORM_INCLUDES}")
if (ANDROID) #android does not have glob.h in /usr/include
    list(APPEND MEGASDK_INCLUDES ../third_party/glob) #temporary hack until code in the sdk depending on glob.h is removed from android build
endif()

#Find mpEnc
if(DEFINED ENV{MPENC_INCLUDE})
    set(MPENC_INCLUDE_DIR $ENV{MPENC_INCLUDE})
else()
    message(FATAL_ERROR "MPENC_INCLUDE env variable not set.")
endif()

if(DEFINED ENV{MPENC_LIB_DIR})
    message("found $ENV{MPENC_LIB_DIR}")
    set(MPENC_LIB_DIR $ENV{MPENC_LIB_DIR})
    message("${MPENC_LIB_DIR}")
else()
    message(FATAL_ERROR "MPENC_LIB_DIR env varibale not set.")
endif()
#===


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
    ${OPENSSL_INCLUDE_DIR}
    ${MPENC_INCLUDE_DIR}
)

add_library(chatclient ${SRCS})
target_link_libraries(chatclient
    rtcmodule
    ${MEGASDK_LIB}
    ${CRYPTOPP_LIBRARIES}
)


