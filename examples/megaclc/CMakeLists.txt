
add_executable(megaclc)

target_sources(megaclc
    PRIVATE
    mclc_autocompletion.h
    mclc_chat_and_call_actions.h
    mclc_commands.h
    mclc_enums_to_string.h
    mclc_general_utils.h
    mclc_globals.h
    mclc_listeners.h
    mclc_logging.h
    mclc_no_interactive.h
    mclc_prompt.h
    mclc_reports.h
    mclc_resources.h

    megaclc.cpp
    mclc_autocompletion.cpp
    mclc_chat_and_call_actions.cpp
    mclc_commands.cpp
    mclc_enums_to_string.cpp
    mclc_general_utils.cpp
    mclc_globals.cpp
    mclc_listeners.cpp
    mclc_logging.cpp
    mclc_no_interactive.cpp
    mclc_prompt.cpp
    mclc_reports.cpp
    mclc_resources.cpp
)

find_package(Boost REQUIRED COMPONENTS program_options)

target_link_libraries(megaclc
    PRIVATE
    Boost::boost
    Boost::program_options
    MEGA::CHATlib
)

## Adjust compilation flags for warnings and errors ##
target_platform_compile_options(
    TARGET megaclc
    UNIX $<$<CONFIG:Debug>:-ggdb3> -Wall -Wextra -Wconversion -Wno-unused-parameter
)

if(ENABLE_CHATLIB_WERROR)
    target_platform_compile_options(
        TARGET megaclc
        UNIX  $<$<CONFIG:Debug>: -Werror
                                 -Wno-error=deprecated-declarations> # Kept as a warning, do not promote to error.
    )
endif()
