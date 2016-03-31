# - Find LibEvent (a cross-platform event library)
# This module defines
# LIBEVENT_INCLUDE_DIRS, where to find LibEvent headers
# LIBEVENT_LIBRARIES, LibEvent libraries
# LIBEVENT_FOUND, If false, do not try to use libevent

find_package(PkgConfig)
pkg_check_modules(PC_LIBEVENT QUIET libevent)

find_path(LIBEVENT_INCLUDE_DIRS event.h
    HINTS ${PC_LIBEVENT_INCLUDEDIR} ${PC_LIBEVENT_INCLUDE_DIRS}
)
find_library(LIBEVENT_LIB NAMES event
    HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
)
find_library(LIBEVENT_LIB_CORE NAMES event_core
    HINTS ${LC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
)

find_library(LIBEVENT_LIB_EXTRA NAMES event_extra
    HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
)
# On windows for some reason there are no distinct openssl and threading libs
if (NOT WIN)
    find_Library(LIBEVENT_LIB_PTHREADS NAMES event_pthreads
        HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
    )
    find_library(LIBEVENT_LIB_OPENSSL NAMES event_openssl
        HINTS ${PC_LIBEVENT_LIBDIR} ${PC_LIBEVENT_LIBRARY_DIRS}
    )
endif()

if (LIBEVENT_LIB AND LIBEVENT_LIB_CORE AND LIBEVENT_LIB_EXTRA)
    set(LIBEVENT_LIBRARIES ${LIBEVENT_LIB} ${LIBEVENT_LIB_CORE} ${LIBEVENT_LIB_EXTRA})
    if (NOT WIN)
        list(APPEND LIBEVENT_LIBRARIES ${LIBEVENT_LIB_OPENSSL} ${LIBEVENT_LIB_PTHREADS})
    endif()
    message(STATUS "libevent libs: ${LIBEVENT_LIBRARIES}")
else()
    message(WARNING "libevent libs not found")
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBEVENT_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEvent DEFAULT_MSG
    LIBEVENT_LIBRARIES LIBEVENT_INCLUDE_DIRS)

mark_as_advanced(LIBEVENT_LIB LIBEVENT_LIB_CORE LIBEVENT_LIB_EXTRA)
if (NOT WIN)
    mark_as_advanced(LIBEVENT_LIB_OPENSSL LIBEVENT_LIB_PTHREADS)
endif()

