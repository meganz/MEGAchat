cmake_minimum_required(VERSION 2.8)
project(karere)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include (utils.cmake)
find_package(Cryptopp REQUIRED)
find_package(Mega REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Sqlite3 REQUIRED)

set(optKarereBuildShared 0 CACHE BOOL "Build libkarere as a shared library")
set(KARERE_LOGGER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/base CACHE PATH "Karere logger include dir") #tell mpenc to use the karere logger
set(LIBWS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/libws CACHE PATH "libws source dir, where its CMakeLists.txt is located")
set(LIBWS_EXTERNAL_LOOP 1 CACHE BOOL "Use libws with external libevent loop")

set (SRCS
    karereCommon.cpp
    base64.cpp
    strophe.disco.cpp
    dummyCrypto.cpp
    megaCryptoFunctions.cpp
    chatClient.cpp
    userAttrCache.cpp
    contactList.cpp
    url.cpp
    chatd.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    strongvelope/strongvelope.cpp
    presenced.cpp
    megachatapi.cpp
    megachatapi_impl.cpp
)

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
    if (CLANG_STDLIB)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    endif()
endif()

#seems _WIN32 is not seen in libevent dns header
if (WIN32)
    add_definitions(-D_WIN32 -DWIN32)
endif()
add_subdirectory(rtcModule)
add_subdirectory(${LIBWS_DIR} libws)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/genDbSchema.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dbSchema.sql
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

#===
# The public variables, such as KARERE_INCLIDE_DIRS should not contain relative paths, as they
# can be used outside of this context, and the base directory would be another one.
set(KARERE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base
    ${RTCMODULE_INCLUDE_DIRS}
    ${STROPHE_INCLUDE_DIRS}
    ${SERVICES_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party

    ${LIBMEGA_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${LIBWS_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/libws
    CACHE STRING "Include directries needed by projects using karere lib"
)

include_directories(${KARERE_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIR})
if (optKarereBuildShared)
    add_library(karere SHARED ${SRCS})
    # all Karere symbols can be used by the application
    # FIXME: This causes linker warnings about different visibility settings
    set_target_properties(karere PROPERTIES COMPILE_FLAGS "-fvisibility=default")
else()
    add_library(karere ${SRCS})
endif()
set_target_properties(karere PROPERTIES COMPILE_FLAGS "${LIBMEGA_DEFINES}")
target_link_libraries(karere
    rtcmodule
    ${LIBMEGA_LIBRARIES}
    ws
    ${SQLITE3_LIBRARY}
)


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
