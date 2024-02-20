
# MEGAChat specific options
option(USE_WEBRTC "Support for voice and/or video calls" OFF)
option(ENABLE_QT_BINDINGS "Enable the target to build the Qt Bindings" ON)

# MEGAsdk options
# Configure MEGAsdk specific options for MEGAchat and then load the rest of MEGAsdk configuration
set(ENABLE_CHAT ON) # Chat management functionality.
set(USE_LIBUV ON) # Includes the library and turns on internal web and ftp server functionality.
include(sdklib_options)
