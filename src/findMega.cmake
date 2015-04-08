cmake_minimum_required(VERSION 2.8)

#find Mega SDK and its dependencies
find_path(MEGASDK_PUBLIC_INCLUDE_DIR megaapi.h)
if (NOT MEGASDK_PUBLIC_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find mega sdk header megaapi.h")
endif()

find_library(MEGASDK_LIBS mega)
if (NOT MEGASDK_LIBS)
    message(FATAL_ERROR "Could not find mega sdk lib (but header found)")
endif()

if (NOT WIN32)
    find_library(CARES_LIB cares)
    if (NOT CARES_LIB)
        message(FATAL_ERROR "Could not find c-ares library, neede by Mega SDK")
    endif()
    #c-ares is the only megasdk dependency that we don't already include,
    #so add it to the megasdk libs
    list(APPEND MEGASDK_LIBS ${CARES_LIB})
endif()

#add mega platform includes
find_path(
    MEGASDK_PLATFORM_INCLUDES
    NAMES megawaiter.h meganet.h
    PATHS "${MEGASDK_PUBLIC_INCLUDE_DIR}/mega"
    PATH_SUFFIXES wp8 win32 posix
)
set(MEGASDK_INCLUDES "${MEGASDK_PUBLIC_INCLUDE_DIR}" "${MEGASDK_PUBLIC_INCLUDE_DIR}/mega" "${MEGASDK_PLATFORM_INCLUDES}")

#if (ANDROID) #android does not have glob.h in /usr/include
#    list(APPEND MEGASDK_INCLUDES ../third_party/glob) #temporary hack until code in the sdk depending on glob.h is removed from android build
#endif()

#save globally so other cmakefiles can use these
set_property(GLOBAL PROPERTY MEGASDK_INCLUDES ${MEGASDK_INCLUDES})
set(MEGASDK_LIBS ${MEGASDK_LIBS})

