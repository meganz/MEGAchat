cmake_minimum_required(VERSION 2.8)
project(karere)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not set, defaulting to Debug")
endif()

include (utils.cmake)

set(optKarereBuildShared 0 CACHE BOOL "Build libkarere as a shared library")
set(optKarereDisableWebrtc 1 CACHE BOOL "Disable webrtc")
set(optKarereUseLibwebsockets 0 CACHE BOOL "Use libwebsockets + libuv")

find_package(Cryptopp REQUIRED)
#force Mega headers to enable cryptopp stuff
add_definitions(-DUSE_CRYPTOPP)
find_package(Mega REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Sqlite3 REQUIRED)


set(KARERE_LOGGER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/base CACHE PATH "Karere logger include dir") #tell mpenc to use the karere logger

set (SRCS
    karereCommon.cpp
    base64url.cpp
    chatClient.cpp
    userAttrCache.cpp
    url.cpp
    chatd.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    strongvelope/strongvelope.cpp
    presenced.cpp
    megachatapi.cpp
    megachatapi_impl.cpp 
    net/websocketsIO.cpp   
)

if (optKarereUseLibwebsockets)
    list(APPEND SRCS waiter/libuvWaiter.cpp net/libwebsocketsIO.cpp)	
endif()

if (NOT optKarereDisableWebrtc)
    list(APPEND SRCS rtcCrypto.cpp)
endif()

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
    add_definitions(-Wall -Wno-unused-local-typedefs)
    if (optAsanMode AND ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug"))
        add_definitions(-fsanitize=${optAsanMode} -fno-omit-frame-pointer)
        if (optKarereBuildShared)
            set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=${optAsanMode}")
        endif()
    endif()
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GET_APPDATA_DIR_WEAKLINK_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")   
	if (optKarereUseLibwebsockets)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_LIBWEBSOCKETS=1") 
	endif()
    if (CLANG_STDLIB)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    endif()
endif()

if (WIN32)
    add_definitions(-D_WIN32 -DWIN32)
endif()

set(KARERE_DEFINES -DHAVE_KARERE_LOGGER ${LIBMEGA_DEFINES})

if (NOT optKarereDisableWebrtc)
    add_subdirectory(rtcModule)
else()
    add_subdirectory(base)
    list(APPEND KARERE_DEFINES -DKARERE_DISABLE_WEBRTC=1)
endif()

get_property(SERVICES_INCLUDE_DIRS GLOBAL PROPERTY SERVICES_INCLUDE_DIRS)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/genDbSchema.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dbSchema.sql ${CMAKE_CURRENT_SOURCE_DIR}/genDbSchema.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

#===
# The public variables, such as KARERE_INCLIDE_DIRS should not contain relative paths, as they
# can be used outside of this context, and the base directory would be another one.
set(KARERE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${SERVICES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party
    ${LIBMEGA_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

set(KARERE_DEP_LIBS
    ${LIBMEGA_LIBRARIES}
    ${SQLITE3_LIBRARY}
)

if (optKarereUseLibwebsockets)
    list(APPEND KARERE_DEP_LIBS websockets uv crypto ssl)
else()
    list(APPEND KARERE_DEP_LIBS ws)
endif()

if (NOT optKarereDisableWebrtc)
    list(APPEND KARERE_INCLUDE_DIRS ${RTCMODULE_INCLUDE_DIRS} ${WEBRTC_INCLUDES})
    list(APPEND KARERE_DEP_LIBS rtcmodule)
else()
    list(APPEND KARERE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/rtcModule)
    list(APPEND KARERE_DEP_LIBS services)
endif()

set_property(GLOBAL PROPERTY KARERE_INCLUDE_DIRS ${KARERE_INCLUDE_DIRS})
set_property(GLOBAL PROPERTY KARERE_DEFINES ${KARERE_DEFINES})

include_directories(${KARERE_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIR})
add_definitions(${KARERE_DEFINES})

if (optKarereBuildShared)
    add_library(karere SHARED ${SRCS})
    # all Karere symbols can be used by the application
    # FIXME: This causes linker warnings about different visibility settings
    set_target_properties(karere PROPERTIES COMPILE_FLAGS "-fvisibility=default")
else()
    add_library(karere ${SRCS})
endif()

target_link_libraries(karere ${KARERE_DEP_LIBS})

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(CMAKE_DOC_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
    set(CMAKE_DOXYPROJECT_NAME "Karere")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)
