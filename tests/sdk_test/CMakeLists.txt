cmake_minimum_required(VERSION 3.0)
project(sdk_test)

set(CMAKE_BUILD_TYPE "Debug")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
    sdk_test.cpp
    ../../third-party/mega/tests/gtestcommon.cpp
)

add_subdirectory(../../src karere)

get_property(KARERE_INCLUDE_DIRS GLOBAL PROPERTY KARERE_INCLUDE_DIRS)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS} ../../third-party/mega/tests)

get_property(KARERE_DEFINES GLOBAL PROPERTY KARERE_DEFINES)
add_definitions(${KARERE_DEFINES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(sdk_test ${SRCS})

target_link_libraries(sdk_test
    karere gmock gtest
    ${SYSLIBS}
)


set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist")
INSTALL(TARGETS sdk_test DESTINATION "${CMAKE_INSTALL_PREFIX}" COMPONENT Runtime)

include(CPack)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/genDbSchema.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dbSchema.sql
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
