TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    ../../../examples/megaclc/megaclc.cpp

TARGET = megaclc

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

DEFINES += LOG_TO_LOGGER

CONFIG += USE_LIBUV
CONFIG += USE_MEGAAPI
CONFIG += USE_MEDIAINFO
CONFIG += USE_LIBWEBSOCKETS
CONFIG += USE_WEBRTC

include(../../../bindings/qt/megachat.pri)
include(../../../third-party/mega/bindings/qt/sdk.pri)

DEPENDPATH += ../../../examples/megaclc
INCLUDEPATH += ../../../examples/megaclc

SOURCES +=  ../../../examples/megaclc/megaclc.cpp

win32 {
    QMAKE_LFLAGS += /LARGEADDRESSAWARE
    QMAKE_LFLAGS_WINDOWS += /SUBSYSTEM:WINDOWS,5.01
    QMAKE_LFLAGS_CONSOLE += /SUBSYSTEM:CONSOLE,5.01
    DEFINES += PSAPI_VERSION=1
}

macx {
    QMAKE_CXXFLAGS += -DCRYPTOPP_DISABLE_ASM -D_DARWIN_C_SOURCE
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6
    QMAKE_CXXFLAGS -= -stdlib=libc++
    QMAKE_LFLAGS -= -stdlib=libc++
    CONFIG -= c++11
    QMAKE_CXXFLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
    QMAKE_LFLAGS += -F /System/Library/Frameworks/Security.framework/
}

