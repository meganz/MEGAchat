cmake_minimum_required(VERSION 2.8)
project(services)
include(../utils.cmake)

set (optStrophePath "~/strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")

set(SRCS
  services.cpp
  services-strophe.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")
find_package(LibEvent REQUIRED)

set(INCLUDES
 ${LIBEVENT_INCLUDE_DIR}
 ${optStrophePath}
 ${optStrophePath}/build/include
 ${LIBEVENT_INCLUDE_DIR}
)
include_directories(${INCLUDES})

add_definitions(-fvisibility=hidden)
list(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_library(services SHARED ${SRCS})
target_link_libraries(services
  ${optStrophePath}/build/libstrophe.so
  ${LIBEVENT_LIB}
  event_pthreads
)
targetExportDefines(services "")
targetExportFullpathIncludes(services ${INCLUDES})
targetExportCompileFlags(services "")
