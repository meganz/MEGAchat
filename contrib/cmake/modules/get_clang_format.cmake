function(get_clang_format)
    message(STATUS "Downloading .clang-format")


    # Define the URL of the file you want to download
    set(FILE_URL "https://raw.githubusercontent.com/meganz/sdk/master/.clang-format")
    
    # Define the output location for the downloaded file
    set(OUTPUT_FILE ${PROJECT_SOURCE_DIR}/.clang-format)
    
    # Download the file
    file(DOWNLOAD ${FILE_URL} ${OUTPUT_FILE}
         SHOW_PROGRESS  # Optionally show download progress
         STATUS status)
    
    # Check if the download was successful
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Download failed with status: ${status}")
    endif()
    
    message(STATUS "File downloaded to: ${OUTPUT_FILE}")
endfunction()
