if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Support for 16 KB devices. Needed if the NDK version is < r28
    set(ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES ON) # Variable for the Android toolchain.
    if (NOT ANDROID_PLATFORM)
        message(WARNING "Android API level not set. It defaults to 28.")
        set(ANDROID_PLATFORM 28) # Variable for the Android toolchain.
    endif()
endif()
