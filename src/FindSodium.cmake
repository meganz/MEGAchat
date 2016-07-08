find_package(PkgConfig)
pkg_check_modules(PC_SODIUM QUIET libsodium)

find_path(LIBSODIUM_INCLUDE_DIRS sodium.h
    HINTS ${PC_SODIUM_INCLUDEDIR} ${PC_SODIUM_INCLUDE_DIRS}
)
find_library(LIBSODIUM_LIBRARIES NAMES sodium
    HINTS ${PC_SODIUM_LIBDIR} ${PC_SODIUM_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBSODIUM_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibSodium DEFAULT_MSG
    LIBSODIUM_LIBRARIES LIBSODIUM_INCLUDE_DIRS)
