macro(load_chatlib_libraries)

    find_package(libwebsockets CONFIG REQUIRED)
    target_link_libraries(CHATlib PRIVATE websockets)

    find_package(webrtc CONFIG REQUIRED)
    target_link_libraries(CHATlib PUBLIC WEBRTC::webrtc)

endmacro()
