cmake_minimum_required(VERSION 2.8)
project(services)
include(../utils.cmake)

set (optStrophePath "~/strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")

set(SRCS
  cservices.cpp
  cservices-strophe.cpp
  cservices-dns.cpp
  cservices-http.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_package(LibEvent REQUIRED)
find_package(CURL REQUIRED)

add_subdirectory(${optStrophePath} strophe)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)

set(INCLUDES
 ${LIBEVENT_INCLUDE_DIR}
 ${CURL_INCLUDE_DIRS}
 ${STROPHE_INCLUDES}
)
include_directories(${INCLUDES})

if  (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    add_definitions(-fvisibility=hidden -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(services SHARED ${SRCS})
target_link_libraries(services
  strophe
  ${LIBEVENT_LIB}
  ${CURL_LIBRARIES}
  event_pthreads
  z
)
targetExportDefines(services "")
targetExportFullpathIncludes(services ${INCLUDES})
targetExportCompileFlags(services "")
