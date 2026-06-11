find_package(GTest CONFIG REQUIRED)

add_executable(megachat_unit_test)

target_sources(megachat_unit_test
    PRIVATE
        src/sfu_test.cpp
)

target_link_libraries(megachat_unit_test PRIVATE MEGA::CHATlib GTest::gmock GTest::gtest_main)
if(NOT WIN32)
    target_compile_options(megachat_unit_test PRIVATE
        -fsanitize=address -fno-omit-frame-pointer -fno-common)
    target_link_options(megachat_unit_test PRIVATE -fsanitize=address)
endif()

gtest_discover_tests(megachat_unit_test)
