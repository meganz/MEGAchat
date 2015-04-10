cmake_minimum_required(VERSION 2.8)
project(karere)
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

set(KARERE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${RTCMODULE_INCLUDE_DIRS}
    ${STROPHE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party

    ${LIBMEGA_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MPENC_INCLUDE_DIRS}
    CACHE STRING "Include directries needed by projects using karere lib"
)
include_directories(${KARERE_INCLUDE_DIRS})

add_library(karere ${SRCS})

target_link_libraries(karere
    rtcmodule
    ${LIBMEGA_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    mpEnc_cpp
)


