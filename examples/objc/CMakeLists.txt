cmake_minimum_required(VERSION 2.8)
project(objctestapp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
     AppDelegate.m
     ViewController.m
     main.m
     ../src/videoRenderer_objc.mm
     ../src/karereCommon.cpp
)
set(RESOURCES
    gui.storyboard
    LaunchScreen.xib
    Entitlements.plist
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../src chatclient)
add_subdirectory(${MPENC_PATH} mpenc)

get_property(SERVICES_INCLUDES GLOBAL PROPERTY SERVICES_INCLUDES)
get_property(MEGASDK_INCLUDES GLOBAL PROPERTY MEGASDK_INCLUDES)

set(MPENC_PATH "~/mpenc_cpp" CACHE PATH "mpenc dir")
set(MPENC_INCLUDE_DIR ${MPENC_PATH}/src)
set(MPENC_LIB ${MPENC_PATH}/build/src/libmpenc${CMAKE_SHAREDELIBRARY_SUFFIX})

#find libsodium
find_path(SODIUM_INCLUDE_DIR sodium.h
          PATH_SUFFIXES sodium)
find_library(SODIUM_LIBRARY NAMES libsodium sodium)

if(NOT SODIUM_INCLUDE_DIR)
    message(FATAL_ERROR "libsodium include dir not found")
endif()
if(NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "libsodium library not found")
endif()
#===

include_directories(
    ../src
    ../src/base
    ../src/rtcModule
    ../third-party
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SERVICES_INCLUDES}
    ${MEGASDK_INCLUDES}
    ${SODIUM_INCLUDE_DIR}
    ${MPENC_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(objctestapp ${SRCS} ${RESOURCES})
set_target_properties(objctestapp PROPERTIES RESOURCE "${RESOURCES}")
 set_target_properties(objctestapp PROPERTIES
     MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME}, Copyright 2015 Mega LTD."
     MACOSX_BUNDLE_ICON_FILE ""
     MACOSX_BUNDLE_GUI_IDENTIFIER "${PROJECT_NAME}"
     MACOSX_BUNDLE_LONG_VERSION_STRING "${MACOSX_BUNDLE_INFO_STRING}"
     MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
     MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_STRING}"
     MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION_STRING}"
     MACOSX_BUNDLE_COPYRIGHT "Copyright 2015, MEGA Ltd. All Rights Reserved."
 )
 
target_link_libraries(objctestapp
    chatclient
    ${SYSLIBS}
    ${MPENC_LIBRARY}
    "-framework CoreGraphics"
)



