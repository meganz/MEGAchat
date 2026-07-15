add_executable(megachat_unit_test)

target_sources(megachat_unit_test
    PRIVATE
        src/main.cpp
        src/jsonutils_test.cpp
        src/sfu_test.cpp
)

target_link_libraries(megachat_unit_test 
    PRIVATE 
        MEGA::CHATlib 
        MEGA::test_common 
)
if(NOT WIN32)
    target_compile_options(megachat_unit_test PRIVATE
        -fsanitize=address -fno-omit-frame-pointer -fno-common)
    target_link_options(megachat_unit_test PRIVATE -fsanitize=address)
endif()
