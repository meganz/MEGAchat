cmake_minimum_required(VERSION 2.8)
project(services)
include(../utils.cmake)

set(optServicesBuildShared 0 CACHE BOOL "Build libservices as a shared lib, for use of the async services by several shared objects")
set(optAsanMode "" CACHE STRING "Build with AddressSanitizer, in the specified mode (-fsanitize=<mode>, i.e. address,memory) Requires GCC>= 4.9 or Clang>=3.5")

set(SRCS
  cservices.cpp
  logger.cpp
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_package(CURL REQUIRED)

set(SERVICES_INCLUDE_DIRS
	${CURL_INCLUDE_DIRS}
   ${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(GLOBAL PROPERTY SERVICES_INCLUDE_DIRS ${SERVICES_INCLUDE_DIRS})
include_directories(${SERVICES_INCLUDE_DIRS})

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    add_definitions(-fvisibility=hidden -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

    if (optAsanMode AND (${CMAKE_BUILD_TYPE} STREQUAL "Debug"))
        add_definitions(-fsanitize=${optAsanMode} -fno-omit-frame-pointer)
        if (optServicesBuildShared)
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=${optAsanMode}")
        endif()
    endif()
endif()

if (WIN32)
#    set(SYSLIBS zlib)
else()
    set(SYSLIBS z)
    if (NOT APPLE)
        list(APPEND SYSLIBS pthread)
    endif()
endif()

if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    list(APPEND SYSLIBS ${CLANG_STDLIB})
endif()

# We need to export getAppDataDir() from the app executable back to the logger lib,
# so that the logger knows where to create the log file.
# If the logger and the app are in the same module, no special processing is needed.
# But if the logger is in another module (i.e. libservices is a shared lib),
# then we need to do it via a weak symbol

if (APPLE)
    set(GET_APPDATA_DIR_WEAKLINK_FLAGS "-Wl,-U,__ZN6karere9getAppDirEv" CACHE "" INTERNAL FORCE)
elseif(NOT WIN32)
    set(GET_APPDATA_DIR_WEAKLINK_FLAGS "-Wl,-u,__ZN6karere9getAppDirEv" CACHE "" INTERNAL FORCE)
else()
    set(GET_APPDATA_DIR_WEAKLINK_FLAGS CACHE "" INTERNAL FORCE)
    message(WARNING "Don't know how to export a weak symbol on this platform, getAppDataDir() may fail to be exported by app executable if linking dynamically")
endif()

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS /wd4800 /wd4996)
endif()

if (optServicesBuildShared)
    add_definitions(-DMEGAIO_DLL)
# Put the application-provided std::string karere::getAppDir() function
# (declared as a weak symbol in loggerChannelConfig.h) in the import table
# of the libservices shared object instead of looking for its definition
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GET_APPDATA_DIR_WEAKLINK_FLAGS}")
    add_library(services SHARED ${SRCS})
else()
    add_library(services ${SRCS})
endif()

set(SERVICES_DEP_LIBS
  ${CURL_LIBRARIES}
  ${SYSLIBS}
)

target_link_libraries(services ${SERVICES_DEP_LIBS})
