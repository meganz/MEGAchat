if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Ensure that compatibility with Android devices that use a 16KiB page size is enabled
    set(ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES ON)
    set(CMAKE_SYSTEM_VERSION 28)
endif()
