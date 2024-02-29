macro(process_chatlib_vcpkg_libraries)

    if (USE_WEBRTC)
        list(APPEND VCPKG_MANIFEST_FEATURES "use-webrtc")
    else()
        list(APPEND VCPKG_MANIFEST_FEATURES "no-webrtc")
    endif()

    if (ENABLE_CHATLIB_TESTS)
        list(APPEND VCPKG_MANIFEST_FEATURES "sdk-tests")
    endif()

endmacro()
