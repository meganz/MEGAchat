# 
# Main CMakeLists.txt for the MEGAchat library target (MEGA::CHATlib)
# 

add_library(CHATlib)
add_library(MEGA::CHATlib ALIAS CHATlib)

set(CHATLIB_PUB_HEADERS
    megachatapi.h
)

set(CHATLIB_HEADERS
    autoHandle.h
    base64url.h
    buffer.h
    chatclientDb.h
    chatClient.h
    chatdDb.h
    chatd.h
    chatdICrypto.h
    chatdMsg.h
    db.h
    IGui.h
    karereCommon.h
    karereId.h
    megachatapi_impl.h
    presenced.h
    rtcCrypto.h
    sdkApi.h
    sfu.h
    stringUtils.h
    url.h
    userAttrCache.h
)

set(CHATLIB_SOURCES
    base64url.cpp
    chatClient.cpp
    chatclientDb.cpp
    chatd.cpp
    karereCommon.cpp
    kareredb.cpp
    megachatapi.cpp
    megachatapi_impl.cpp
    presenced.cpp
    sfu.cpp
    url.cpp
    userAttrCache.cpp
    karereDbSchema.cpp
)

target_sources(CHATlib
    PRIVATE
    ${CHATLIB_HEADERS}
    ${CHATLIB_SOURCES}
    ${CHATLIB_PUB_HEADERS}
)

# Files by platform and/or feature
# Files should appear only once.
# If the FLAG is not true for a file, it will be added as non-buildable source despite then the file is added again as a buildable one.
target_sources_conditional(CHATlib
    FLAG USE_WEBRTC
    PRIVATE
    rtcCrypto.cpp
)

target_sources_conditional(CHATlib
    FLAG ENABLE_CHATLIB_TESTS OR ENABLE_CHATLIB_MEGACLC
    PRIVATE
    async_utils.h
    async_utils.cpp
)

# Generate karereDbSchema.cpp
add_custom_command(
    OUTPUT karereDbSchema.cpp
    COMMAND ${CMAKE_COMMAND} -DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/genDbSchema.cmake
    DEPENDS dbSchema.sql genDbSchema.cmake
)

# Include directories
target_include_directories(CHATlib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ../third-party
)

# Include modules in subdirectories with sources for CHAtlib
include(base/base.cmake)
include(net/net.cmake)
include(rtcModule/rtcModule.cmake)
include(strongvelope/strongvelope.cmake)
include(waiter/waiter.cmake)

set_target_properties(CHATlib PROPERTIES
    VERSION ${PROJECT_VERSION}
    DEBUG_POSTFIX "d"
)

if(ENABLE_JAVA_BINDINGS)
    set_target_properties(CHATlib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# Add definitions for the compiler
target_compile_definitions(CHATlib
    PRIVATE
        $<$<BOOL:${ENABLE_CHAT}>:ENABLE_CHAT>
        $<$<BOOL:${WIN32}>:NOMINMAX> # TODO Fix locally
        $<$<BOOL:${WIN32}>:$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:MEGA_FULL_STATIC>>
        $<$<BOOL:${APPLE}>:_DARWIN_C_SOURCE>
    PUBLIC
        $<$<NOT:$<BOOL:${USE_WEBRTC}>>:KARERE_DISABLE_WEBRTC>
)

## Load and link needed libraries for the CHATlib target ##

# Load 3rd parties
include(chatlib_libraries) # Include a macro to load the library dependencies
load_chatlib_libraries()

if(WIN32)
    target_link_libraries(CHATlib PRIVATE
        Dbghelp.lib
    )
elseif(ANDROID)
    target_link_libraries(CHATlib PRIVATE
        log
        OpenSLES
    )
endif()

# Link with the SDK library
target_link_libraries(CHATlib PUBLIC MEGA::SDKlib)

## Adjust compilation flags for warnings and errors ##
target_platform_compile_options(
    TARGET CHATlib
    UNIX $<$<CONFIG:Debug>:-ggdb3> -Wall -Wextra -Wconversion -Wno-unused-parameter
)

if(ENABLE_CHATLIB_WERROR)
    target_platform_compile_options(
        TARGET CHATlib
        UNIX  $<$<CONFIG:Debug>: -Werror
                                 -Wno-error=deprecated-declarations> # Kept as a warning, do not promote to error.
        APPLE $<$<CONFIG:Debug>: -Wno-sign-conversion                 -Wno-overloaded-virtual
                                 -Wno-inconsistent-missing-override   -Wno-unused-variable
                                 -Wno-unused-private-field>
    )
endif()
