macro(load_chatlib_libraries)

    find_package(libwebsockets CONFIG REQUIRED)
    target_link_libraries(CHATlib PRIVATE websockets)

endmacro()
