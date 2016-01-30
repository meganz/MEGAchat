cmake_minimum_required(VERSION 2.8)
project(services)
include(../utils.cmake)

set (optStrophePath "${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")
set (optServicesBuildShared 1 CACHE BOOL "Build libservices as a shared lib, for use of the async services by several shared objects")
set(optAsanMode "" CACHE STRING "Build with AddressSanitizer, in the specified mode (-fsanitize=<mode>, i.e. address,memory) Requires GCC>= 4.9 or Clang>=3.5")

set(SRCS
  cservices.cpp
  cservices-strophe.cpp
  cservices-dns.cpp
  services-dns.cpp
  cservices-http.cpp
  logger.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_package(LibEvent REQUIRED)
find_package(CURL REQUIRED)

add_subdirectory(${optStrophePath} strophe)

set(SERVICES_INCLUDE_DIRS
   ${LIBEVENT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS}
   ${STROPHE_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
   CACHE STRING "Include dirs for libservices dependent projects"
)
include_directories(${SERVICES_INCLUDE_DIRS})
if  (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    add_definitions(-fvisibility=hidden -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

set(SYSLIBS z)

if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    list(APPEND SYSLIBS ${CLANG_STDLIB})
endif()

if (optServicesBuildShared)
    add_definitions(-DMEGAIO_DLL)
    add_library(services SHARED ${SRCS})
else()
    add_library(services ${SRCS})
endif()

target_link_libraries(services
  strophe
  ${LIBEVENT_LIBRARIES}
  ${CURL_LIBRARIES}
  ${SYSLIBS}
)
