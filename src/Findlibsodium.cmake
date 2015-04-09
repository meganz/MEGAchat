cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(PC_LIBSODIUM QUIET libsodium)
set(LIBSODIUM_DEFINITIONS ${PC_LIBSODIUM_CFLAGS_OTHER})

find_path(LIBSODIUM_INCLUDE_DIR NAMES sodium.h
    HINTS ${PC_LIBSODIUM_INCLUDEDIR} ${PC_LIBSODIUM_INCLUDE_DIRS}
    PATH_SUFFIXES sodium
)
find_library(LIBSODIUM_LIB NAMES sodium
    HINTS ${PC_LIBSODIUM_LIBDIR} ${PC_LIBSODIUM_LIBRARY_DIRS}
    PATH_SUFFIXES sodium
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBSODIUM_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(sodium DEFAULT_MSG
    LIBSODIUM_LIB LIBSODIUM_INCLUDE_DIR)

set(LIBSODIUM_INCLUDE_DIRS ${LIBSODIUM_INCLUDE_DIR})
set(LIBSODIUM_LIBRARIES ${LIBSODIUM_LIB})
mark_as_advanced(LIBSODIUM_INCLUDE_DIR LIBSODIUM_LIBS)

