cmake_minimum_required(VERSION 2.8)
project(karere)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include (utils.cmake)
find_package(Cryptopp REQUIRED)
find_package(Mega REQUIRED)
find_package(OpenSSL REQUIRED)

set(MPENC_DIR "~/mpenc_cpp" CACHE PATH "mpEnc checkout dir built within a /build subdir")
set(KARERE_LOGGER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/base CACHE PATH "Karere logger include dir") #tell mpenc to use the karere logger
set(LIBWS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/libws CACHE PATH "libws source dir, where its CMakeLists.txt is located")
set(LIBWS_EXTERNAL_LOOP 1 CACHE BOOL "Use libws with external libevent loop")

set (SRCS
    ../third-party/murmurHash/MurmurHash3.cpp
//    karereCommon.cpp
    base64.cpp
    strophe.disco.cpp
    dummyCrypto.cpp
    megaCryptoFunctions.cpp
    chatClient.cpp
    textModule.cpp
    contactList.cpp
    chatd.cpp
    ./text_filter/text_filter.cpp
    ./text_filter/default_link_handler.cpp
    ./text_filter/default_emoticon_handler.cpp
)

add_definitions(-fvisibility=hidden -Wall)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
endif()

add_subdirectory(rtcModule)
add_subdirectory(${MPENC_DIR} mpenc)
add_subdirectory(${LIBWS_DIR} libws)

#===
# The public variables, such as KARERE_INCLIDE_DIRS should not contain relative paths, as they
# can be used outside of this context, and the base directory would be another one.
set(KARERE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${RTCMODULE_INCLUDE_DIRS}
    ${STROPHE_INCLUDE_DIRS}
    ${SERVICES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party

    ${LIBMEGA_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${MPENC_INCLUDE_DIRS}
    ${LIBWS_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/libws
    CACHE STRING "Include directries needed by projects using karere lib"
)

include_directories(${KARERE_INCLUDE_DIRS})
add_library(karere ${SRCS})
target_link_libraries(karere
    rtcmodule
    ${LIBMEGA_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    mpEnc_cpp
    ws
)


