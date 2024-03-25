macro(load_chatlib_libraries)

    find_package(libwebsockets CONFIG REQUIRED)
    target_link_libraries(CHATlib PRIVATE websockets)

    if (USE_WEBRTC)
        find_package(webrtc CONFIG REQUIRED)
        find_package(X11 REQUIRED)
        target_link_libraries(CHATlib PUBLIC
            WEBRTC::webrtc
            X11
            Xfixes
            Xdamage
            Xrandr
            Xtst
            Xcomposite
            Xext
            )
    endif()

endmacro()
