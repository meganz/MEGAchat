cmake_minimum_required(VERSION 3.0)
project(apitestapp)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
    test.cpp
)

add_subdirectory(../../src karere)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS})
add_definitions(-DHAVE_KARERE_LOGGER ${LIBMEGA_DEFINES})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(apitestapp ${SRCS})

target_link_libraries(apitestapp
    karere
    ${SYSLIBS}
)


set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/dist")
INSTALL(TARGETS apitestapp DESTINATION "${CMAKE_INSTALL_PREFIX}" COMPONENT Runtime)

include(CPack)

