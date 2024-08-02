function(get_clang_format)
    message(STATUS "Downloading .clang-format")

    # Download SDK repo archive with only .clang-format
    execute_process(
        COMMAND git archive --output=clang-format.tar --remote=git@code.developers.mega.co.nz:sdk/sdk.git HEAD .clang-format
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_QUIET
    )

    # Extract archive
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xvf clang-format.tar
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_QUIET
    )

    # Remove archive
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E remove clang-format.tar
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_QUIET
    )
endfunction()
