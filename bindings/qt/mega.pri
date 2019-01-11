debug_and_release {
    CONFIG -= debug_and_release
    CONFIG += debug_and_release
}
CONFIG(debug, debug|release) {
    CONFIG -= debug release
    CONFIG += debug
}
CONFIG(release, debug|release) {
    CONFIG -= debug release
    CONFIG += release
}

CONFIG += USE_LIBUV
CONFIG += USE_MEGAAPI
CONFIG += USE_MEDIAINFO
CONFIG += ENABLE_CHAT
CONFIG += USE_WEBRTC
TEMPLATE = app
DEFINES += LOG_TO_LOGGER

win32 {
    QMAKE_LFLAGS += /LARGEADDRESSAWARE
    QMAKE_LFLAGS_WINDOWS += /SUBSYSTEM:WINDOWS,5.01
    QMAKE_LFLAGS_CONSOLE += /SUBSYSTEM:CONSOLE,5.01
    DEFINES += PSAPI_VERSION=1
}

macx {
    QMAKE_CXXFLAGS += -DCRYPTOPP_DISABLE_ASM -D_DARWIN_C_SOURCE
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
    QMAKE_LFLAGS += -F /System/Library/Frameworks/Security.framework/
    DEFINES += WEBRTC_MAC
}

unix:debug|macx:debug {
    CONFIG += sanitizer sanitize_address
    CONFIG += QMAKE_COMMON_SANITIZE_CFLAGS
}
else {
    CONFIG -= sanitizer sanitize_address
    CONFIG -= QMAKE_COMMON_SANITIZE_CFLAGS
}
