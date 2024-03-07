
add_executable(megachat_tests)

target_sources(megachat_tests
    PRIVATE
    sdk_test.h
    ../../third-party/mega/tests/gtest_common.h # TODO From SDK utils target
    ../../third-party/mega/tests/integration/sdk_test_utils.h # TODO From SDK utils target

    sdk_test.cpp
    ../../third-party/mega/tests/gtest_common.cpp # TODO From SDK utils target
    ../../third-party/mega/tests/integration/sdk_test_utils.cpp # TODO From SDK utils target
)

# Load and link needed libraries for the CHATlib tests
find_package(GTest CONFIG REQUIRED)

target_link_libraries(megachat_tests
    PRIVATE
    GTest::gmock
    GTest::gtest
    MEGA::CHATlib
)

# TODO From SDK utils target
target_include_directories(megachat_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/mega/tests/
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
