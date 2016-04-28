cmake_minimum_required(VERSION 2.8)
find_package(PkgConfig)
pkg_check_modules(PC_LIBMEGA QUIET libmega)

#find Mega SDK and its dependencies
find_path(LIBMEGA_PUBLIC_INCLUDE_DIR megaapi.h
    HINTS ${PC_LIBMEGA_INCLUDEDIR} ${PC_LIBMEGA_INCLUDE_DIRS}
)

find_library(_LIBMEGA_LIBRARIES mega
    HINTS ${PC_LIBMEGA_LIBDIR} ${PC_LIBMEGA_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibMega DEFAULT_MSG
    _LIBMEGA_LIBRARIES LIBMEGA_PUBLIC_INCLUDE_DIR
)

# For the libs that don't have FindXXX cmake modules
macro(findlib varprefix name header libnames)
    pkg_check_modules("${varprefix}" QUIET "${name}")
    if (NOT ${name}_FOUND)
        #look in the standard paths, can be set on the commandline via -DCMAKE_PREFIX_PATH=... or -DCMAKE_FIND_ROOT=...
        set(flhdr "${varprefix}_INCLUDE_DIRS")
        find_path("${flhdr}" "${header}")
        if ("${${flhdr}}" STREQUAL "${flhdr}-NOTFOUND")
            message(FATAL_ERROR "ERROR: library ${name} not found")
        else()
            # lib header found in standard paths, possibly in CMAKE_PREFIX_PATH,
            # so it should work out of the box as long as we add CMAKE_PREFIX_PATH
            # to the include and library dirs that are passed to the compiler.
            # No need to set ${varprefix}_INCLUDE_DIRS
            # quite possibly, lib files will also be in standard paths,
            # so we just put their names without paths
            set(${varprefix}_LIBRARIES ${libnames})
            message(STATUS "Found ${name}")
        endif()
    endif()
endmacro()

findlib(SQLITE sqlite3 sqlite3.h sqlite3)
findlib(CRYPTOPP libcrypto++ "cryptopp/cryptlib.h" cryptopp)
findlib(CARES libcares ares.h cares)
find_package(CURL REQUIRED)

list(APPEND _LIBMEGA_LIBRARIES ${CURL_LIBRARIES} ${CARES_LIBRARIES} ${CRYPTOPP_LIBRARIES}
    ${SQLITE_LIBRARIES})

if (NOT WIN32)
    set(platdir posix)
else()
    if (WINPHONE)
        set(platdir wp8)
    else()
        set(platdir wincurl)
    endif()
endif()

if (APPLE)
#needed for thumbnail generation, we don't use freeimage on these platforms
    list(APPEND _LIBMEGA_LIBRARIES "-framework ImageIO")
    if (APPLE_IOS)
        list(APPEND _LIBMEGA_LIBRARIES "-framework MobileCoreServices")
    endif()
endif()

set(LIBMEGA_INCLUDE_DIRS 
    "${LIBMEGA_PUBLIC_INCLUDE_DIR}"
    "${LIBMEGA_PUBLIC_INCLUDE_DIR}/mega/${platdir}"
    "${CURL_INCLUDE_DIRS}" "${CARES_INCLUDE_DIRS}" "${CRYPTOPP_INCLUDE_DIRS}" "${SQLITE_INCLUDE_DIRS}"
    CACHE STRING "" FORCE
)

set(LIBMEGA_LIBRARIES "${_LIBMEGA_LIBRARIES}" CACHE STRING "libmega library and dependencies" FORCE)
set(LIBMEGA_DEFINES "-DHAVE_CONFIG_H" CACHE STRING "libmega definitions needed for public headers" FORCE)

#if (ANDROID) #android does not have glob.h in /usr/include
#    list(APPEND MEGASDK_INCLUDES ../third_party/glob) #temporary hack until code in the sdk depending on glob.h is removed from android build
#endif()

mark_as_advanced(_LIBMEGA_LIBRARIES)
