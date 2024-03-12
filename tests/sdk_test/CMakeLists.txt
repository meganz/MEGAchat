
add_executable(megachat_tests)

target_sources(megachat_tests
    PRIVATE
    sdk_test.h
    sdk_test.cpp
)

# Load and link needed libraries for the CHATlib tests
find_package(GTest CONFIG REQUIRED)

target_link_libraries(megachat_tests
    PRIVATE
    MEGA::test_tools
    GTest::gmock
    GTest::gtest
    MEGA::CHATlib
)

## Adjust compilation flags for warnings and errors ##
target_platform_compile_options(
    TARGET megachat_tests
    UNIX $<$<CONFIG:Debug>:-ggdb3> -Wall -Wextra -Wconversion -Wno-unused-parameter
)

if(ENABLE_CHATLIB_WERROR)
    target_platform_compile_options(
        TARGET megachat_tests
        UNIX  $<$<CONFIG:Debug>: -Werror
                                 -Wno-error=deprecated-declarations> # Kept as a warning, do not promote to error.
    )
endif()

# Integration tests require the following file to work
file(GLOB TESTING_AUX_FILES "*.png")
add_custom_command(
    TARGET megachat_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${TESTING_AUX_FILES} $<TARGET_FILE_DIR:megachat_tests>
    COMMENT "Copying test files for integration tests."
)
