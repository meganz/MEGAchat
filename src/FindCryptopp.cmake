# - Find cryptopp
# Find the native CRYPTOPP headers and libraries.
#
#  CRYPTOPP_INCLUDE_DIRS - where to find include files
#  CRYPTOPP_LIBRARIES    - List of libraries when using cryptopp.
#  CRYPTOPP_FOUND        - True if cryptopp found.

# The library is called 'crypto++' on Linux, on OS/X it is called 'cryptopp'.
find_package(PkgConfig)
pkg_check_modules(PC_CRYPTOPP libcryptopp)

# Look for the header file.
find_path(CRYPTOPP_INCLUDE_DIRS NAMES sha.h PATH_SUFFIXES crypto++ cryptopp
    HINTS ${PC_CRYPTOPP_INCLUDEDIR} ${PC_CRYPTOPP_INCLUDE_DIRS}
)
# Look for the library.
find_library(CRYPTOPP_LIBRARIES NAMES cryptopp
    HINTS ${PC_CRYPTOPP_LIBDIR} ${PC_CRYPTOPP_LIBRARY_DIRS}
)

# handle the QUIETLY and REQUIRED arguments and set CRYPTOPP_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Crypto++ DEFAULT_MSG
    CRYPTOPP_LIBRARIES CRYPTOPP_INCLUDE_DIRS)

set(CRYPTOPP_INCLUDE_DIRS ${CRYPTOPP_INCLUDE_DIRS}/..)
