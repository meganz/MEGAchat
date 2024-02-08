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
    FLAG USE_QT
    PRIVATE
    videoRenderer_Qt.h
    videoRenderer_Qt.cpp
)

target_sources_conditional(CHATlib
    FLAG USE_WEBRTC
    PRIVATE
    rtcCrypto.cpp
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For the top level projects.
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # For the external projects.
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../third-party # rapidjson, header-only lib
)


set_target_properties(CHATlib PROPERTIES
    VERSION ${PROJECT_VERSION}
    DEBUG_POSTFIX "d"
)

# Add definitions for the compiler
target_compile_definitions(CHATlib
    PRIVATE
    $<$<BOOL:${ENABLE_CHAT}>:ENABLE_CHAT>
    $<$<NOT:$<BOOL:${USE_WEBRTC}>>:KARERE_DISABLE_WEBRTC>
)

## Load and link needed libraries for the CHATlib target ##


# Link with the SDK library
target_link_libraries(CHATlib PRIVATE MEGA::SDKlib)

