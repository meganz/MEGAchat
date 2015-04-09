cmake_minimum_required(VERSION 2.8)
find_package(PkgConfig)
pkg_check_modules(PC_LIBMEGA QUIET libmega)

#find Mega SDK and its dependencies
find_path(LIBMEGA_PUBLIC_INCLUDE_DIR megaapi.h
    HINTS ${PC_LIBMEGA_INCUDEDIR} ${PC_LIBMEGA_INCLUDE_DIRS}
)

find_library(LIBMEGA_LIBRARIES mega
    HINTS ${PC_LIBMEGA_LIBDIR} ${PC_LIBMEGA_LIBRARY_DIRS}
)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibMega DEFAULT_MSG
    LIBMEGA_LIBRARIES LIBMEGA_PUBLIC_INCLUDE_DIR
)

if (NOT WIN32)
    pkg_check_modules(PC_CARES QUIET libcares)
    find_library(CARES_LIB cares HINTS ${PC_CARES_LIBDIR} ${PC_CARES_LIBRARY_DIRS})
    if (NOT CARES_LIB)
        message(FATAL_ERROR "Could not find c-ares library, neede by Mega SDK")
    endif()
    #c-ares is the only megasdk dependency that we don't already include,
    #so add it to the megasdk libs
    list(APPEND LIBMEGA_LIBRARIES ${CARES_LIB})
endif()

#add mega platform includes
find_path(
    LIBMEGA_PLATFORM_INCLUDES
    NAMES megawaiter.h meganet.h
    PATHS "${LIBMEGA_PUBLIC_INCLUDE_DIR}/mega"
    PATH_SUFFIXES wp8 win32 posix
)

set(LIBMEGA_INCLUDE_DIRS "${LIBMEGA_PUBLIC_INCLUDE_DIR}"
    "${LIBMEGA_PUBLIC_INCLUDE_DIR}/mega" "${LIBMEGA_PLATFORM_INCLUDES}")

#if (ANDROID) #android does not have glob.h in /usr/include
#    list(APPEND MEGASDK_INCLUDES ../third_party/glob) #temporary hack until code in the sdk depending on glob.h is removed from android build
#endif()

