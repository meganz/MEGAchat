# - Find LibEvent (a cross-platform event library)
# This module defines
# LIBEVENT_INCLUDE_DIRS, where to find LibEvent headers
# LIBEVENT_LIBRARIES, LibEvent libraries
# LIBEVENT_FOUND, If false, do not try to use libevent

if (NOT LIBEVENT_LIBRARIES)

find_package(PkgConfig)
pkg_check_modules(PC_LIBEVENT QUIET libevent)

find_path(LIBEVENT_INCLUDE_DIRS event.h
    HINTS ${PC_LIBEVENT_INCLUDEDIR} ${PC_LIBEVENT_INCLUDE_DIRS}
)
find_library(LIBEVENT_LIB_CORE NAMES event
    HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
)
find_library(LIBEVENT_LIB_THREADS NAMES event_pthreads event_win32
    HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
)
if (LIBEVENT_LIB_CORE)
    set(LIBEVENT_LIBRARIES ${LIBEVENT_LIB_CORE} ${LIBEVENT_LIB_THREADS})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEvent DEFAULT_MSG
    LIBEVENT_LIBRARIES LIBEVENT_INCLUDE_DIRS)

mark_as_advanced(LIBEVENT_LIB_CORE LIBEVENT_LIB_THREADS)

endif()
