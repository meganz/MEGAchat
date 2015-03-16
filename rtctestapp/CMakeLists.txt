cmake_minimum_required(VERSION 2.8)
project(rtctestapp)

FIND_PACKAGE(Qt4 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
     mainwindow.cpp
     main.cpp
     ../src/videoRenderer_Qt.cpp
     ../src/karereCommon.cpp
)

set(QT_FORMS mainwindow.ui)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS} -D_DEBUG -fvisibility=hidden -Wall)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../src chatclient)
get_property(SERVICES_INCLUDES GLOBAL PROPERTY SERVICES_INCLUDES)
get_property(MEGASDK_INCLUDES GLOBAL PROPERTY MEGASDK_INCLUDES)
if(DEFINED ENV{MPENC_INCLUDE})
    set(MPENC_INCLUDE_DIR $ENV{MPENC_INCLUDE})
else()
    message(FATAL_ERROR, "MPENC_INCLUDE env variable not set.")
endif()

if(DEFINED ENV{MPENC_LIB})
    message("found $ENV{MPENC_LIB}")
    set(MPENC_LIB_DIR $ENV{MPENC_LIB})
    message("${MPENC_LIB_DIR}")
else()
    message(FATAL_ERROR, "MPENC_LIB env varibale not set.")
endif()
find_path(SODIUM_INCLUDE_DIR sodium.h
          PATH_SUFFIXES sodium)
find_library(SODIUM_LIBRARY NAMES libsodium sodium)
find_path(EASYLOGGING_INCLUDE_DIR easylogging++.h)

if(NOT SODIUM_INCLUDE_DIR)
    message(FATAL_ERROR "libsodium include dir not found")
endif()

if(NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "libsodium library not found")
endif()

if(NOT EASYLOGGING_INCLUDE_DIR)
    message(FATAL_ERROR "easylogging header not found")
endif()

set(EASYLOGGING_INCLUDE_DIRS ${EASYLOGGING_INCLUDE_DIR})
set(SODIUM_LIBRARIES ${SODIUM_LIBRARY})
set(SODIUM_INCLUDE_DIRS ${SODIUM_INCLUDE_DIR})

include_directories(
    ../src
    ../src/base
    ../src/rtcModule
    ../third-party
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SERVICES_INCLUDES}
    ${MEGASDK_INCLUDES}
    ${EASYLOGGING_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
    ${MPENC_INCLUDE_DIR}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(LIBMPENC_CPP_LIB "libmpEnc_cpp.dylib")
else ()
    set(LIBMPENC_CPP_LIB "libmpEnc_cpp.so")
endif ()

add_executable(rtctestapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
set_target_properties(rtctestapp PROPERTIES AUTOMOC TRUE)
target_link_libraries(rtctestapp
    ${QT_LIBRARIES}
    chatclient
    ${SYSLIBS}
    ${SODIUM_LIBRARIES}
    ${MPENC_LIB_DIR}/${LIBMPENC_CPP_LIB}
)


