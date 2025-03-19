
# MEGAChat specific options
if((CMAKE_SYSTEM_NAME STREQUAL "iOS") OR (UNIX AND NOT APPLE)) # iOS or Linux/Android
    option(USE_WEBRTC "Support for voice and/or video calls" ON)
    if (CMAKE_SYSTEM_NAME STREQUAL "Android")
        option(ENABLE_JAVA_BINDINGS "Enable the target to build the Java Bindings" ON)
    endif()
else() # Win and mac
    option(USE_WEBRTC "Support for voice and/or video calls" OFF)
endif()

option(ENABLE_JAVA_BINDINGS "Enable the target to build the Java Bindings" OFF) # Except Android, where it was already turned ON above

if(ENABLE_CHATLIB_QTAPP)
    option(ENABLE_QT_BINDINGS "Enable the target to build the Qt Bindings" ON)
else()
    option(ENABLE_QT_BINDINGS "Enable the target to build the Qt Bindings" OFF)
endif()

# MEGAsdk options
# Configure MEGAsdk specific options for MEGAchat and then load the rest of MEGAsdk configuration
set(ENABLE_CHAT ON) # Chat management functionality.
if (ENABLE_CHATLIB_QTAPP)
    set(USE_LIBUV ON) # Used by the QtApp: Includes the library and turns on internal web and ftp server functionality in the SDK.
endif()
include(sdklib_options)
