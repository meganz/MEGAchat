function(get_clang_format)
    message(STATUS "Downloading .clang-format")

    set(URL "https://code.developers.mega.co.nz/sdk/sdk/-/raw/develop/.clang-format")
    set(DESTINATION_PATH ${PROJECT_SOURCE_DIR}/.clang-format)
    file(DOWNLOAD ${URL} ${DESTINATION_PATH} SHOW_PROGRESS)
endfunction()
