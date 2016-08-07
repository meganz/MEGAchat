find_package(PkgConfig)
pkg_check_modules(PC_CARES QUIET libcares)

find_path(LIBSODIUM_INCLUDE_DIRS ares.h
    HINTS ${PC_CARES_INCLUDEDIR} ${PC_CARES_INCLUDE_DIRS}
)
find_library(LIBCARES_LIBRARIES NAMES cares
    HINTS ${PC_CARES_LIBDIR} ${PC_CARES_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBSODIUM_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibCares DEFAULT_MSG
    LIBCARES_LIBRARIES LIBCARES_INCLUDE_DIRS)
