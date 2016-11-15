cmake_minimum_required(VERSION 3.0)
project(sdk_test)

set(CMAKE_BUILD_TYPE "Debug")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
    sdk_test.cpp
)

add_subdirectory(../../src karere)
include_directories(${LIBMEGA_PUBLIC_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS})
add_definitions(-DHAVE_KARERE_LOGGER ${LIBMEGA_DEFINES})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(sdk_test ${SRCS})

target_link_libraries(sdk_test
    karere
    ${SYSLIBS}
)


set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist")
INSTALL(TARGETS sdk_test DESTINATION "${CMAKE_INSTALL_PREFIX}" COMPONENT Runtime)

include(CPack)

