cmake_minimum_required(VERSION 2.8)
project(chatclient)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include (utils.cmake)
find_package(Cryptopp REQUIRED)
find_package(Mega REQUIRED)
find_package(OpenSSL REQUIRED)

set(MPENC_DIR "~/mpenc_cpp" CACHE PATH "mpEnc checkout dir built within a /build subdir")

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
add_subdirectory(${MPENC_DIR} mpenc)
#===
message(STATUS "mpenc includes = ${MPENC_INCLUDE_DIRS}")

include_directories(
    .
    ../third-party
    ./rtcmodule #same as ${RTCMODULE_INCLUDE_DIRS} but simpler
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBMEGA_INCLUDE_DIRS}
    ${SERVICES_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MPENC_INCLUDE_DIRS}
)

add_library(chatclient ${SRCS})
target_link_libraries(chatclient
    rtcmodule
    ${MEGASDK_LIB}
    ${CRYPTOPP_LIBRARIES}
    ${MPENC_LIBS}
)


