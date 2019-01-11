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

!release {
    DEFINES += DEBUG
}
else {
    DEFINES += NDEBUG
}

CONFIG += c++11
CONFIG += USE_LIBUV
CONFIG += USE_MEGAAPI
CONFIG += USE_MEDIAINFO
CONFIG += ENABLE_CHAT
CONFIG += USE_WEBRTC

TEMPLATE = app

DEFINES += LOG_TO_LOGGER
DEFINES += ENABLE_CHAT

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

# include the configuration for MEGA SDK
include(../../third-party/mega/bindings/qt/sdk.pri)

MEGACHAT_BASE_PATH = $$PWD/../..

VPATH += $$MEGACHAT_BASE_PATH/src
SOURCES += megachatapi.cpp \
            megachatapi_impl.cpp \
            strongvelope/strongvelope.cpp \
            presenced.cpp \
            base64url.cpp \
            chatClient.cpp \
            chatd.cpp \
            url.cpp \
            karereCommon.cpp \
            userAttrCache.cpp \
            base/logger.cpp \
            base/cservices.cpp \
            net/websocketsIO.cpp \
            karereDbSchema.cpp \
            net/libwebsocketsIO.cpp \
            waiter/libuvWaiter.cpp

HEADERS  += asyncTest-framework.h \
            buffer.h \
            chatd.h \
            karereCommon.h \
            messageBus.h \
            videoRenderer_objc.h \
            asyncTest.h \
            chatClient.h \
            chatdICrypto.h \
            db.h \
            karereId.h \
            presenced.h \
            serverListProvider.h \
            autoHandle.h \
            chatCommon.h  \
            chatdMsg.h \
            dummyCrypto.h  \
            megachatapi.h  \
            rtcCrypto.h \
            stringUtils.h \
            url.h \
            base64url.h \
            chatdDb.h \
            IGui.h \
            megachatapi_impl.h \
            sdkApi.h \
            userAttrCache.h \
            ../bindings/qt/QTMegaChatEvent.h \
            ../bindings/qt/QTMegaChatListener.h \
            ../bindings/qt/QTMegaChatRoomListener.h \
            ../bindings/qt/QTMegaChatRequestListener.h \
            ../bindings/qt/QTMegaChatCallListener.h \
            ../bindings/qt/QTMegaChatVideoListener.h \
            ../bindings/qt/QTMegaChatNotificationListener.h \
            ../bindings/qt/QTMegaChatNodeHistoryListener.h \
            base/asyncTools.h \
            base/addrinfo.hpp \
            base/cservices-thread.h \
            base/cservices.h \
            base/gcmpp.h \
            base/logger.h \
            base/loggerFile.h \
            base/loggerConsole.h \
            base/retryHandler.h \
            base/promise.h \
            base/services.h \
            base/timers.hpp \
            base/trackDelete.h \
            net/libwebsocketsIO.h \
            net/websocketsIO.h \
            rtcModule/IDeviceListImpl.h \
            rtcModule/IRtcCrypto.h \
            rtcModule/IRtcStats.h \
            rtcModule/ITypes.h \
            rtcModule/ITypesImpl.h \
            rtcModule/IVideoRenderer.h \
            rtcModule/messages.h \
            rtcModule/rtcmPrivate.h \
            rtcModule/rtcStats.h \
            rtcModule/streamPlayer.h \
            rtcModule/webrtc.h \
            rtcModule/webrtcAdapter.h \
            rtcModule/webrtcAsyncWaiter.h \
            rtcModule/webrtcPrivate.h \
            strongvelope/tlvstore.h \
            strongvelope/strongvelope.h \
            strongvelope/cryptofunctions.h \
            waiter/libuvWaiter.h

CONFIG(qt) {
  SOURCES += ../bindings/qt/QTMegaChatEvent.cpp \
            ../bindings/qt/QTMegaChatListener.cpp \
            ../bindings/qt/QTMegaChatRoomListener.cpp \
            ../bindings/qt/QTMegaChatRequestListener.cpp \
            ../bindings/qt/QTMegaChatCallListener.cpp \
            ../bindings/qt/QTMegaChatVideoListener.cpp \
            ../bindings/qt/QTMegaChatNotificationListener.cpp \
            ../bindings/qt/QTMegaChatNodeHistoryListener.cpp
}

CONFIG(USE_WEBRTC) {
    SOURCES += rtcCrypto.cpp \
             rtcModule/webrtc.cpp \
             rtcModule/webrtcAdapter.cpp \
             rtcModule/rtcStats.cpp

}
else {
    DEFINES += KARERE_DISABLE_WEBRTC=1 SVC_DISABLE_STROPHE
}

INCLUDEPATH += $$MEGACHAT_BASE_PATH/src
INCLUDEPATH += $$MEGACHAT_BASE_PATH/src/base
INCLUDEPATH += $$MEGACHAT_BASE_PATH/src/rtcModule
INCLUDEPATH += $$MEGACHAT_BASE_PATH/src/strongvelope
INCLUDEPATH += $$MEGACHAT_BASE_PATH/third-party
INCLUDEPATH += $$MEGACHAT_BASE_PATH/bindings/qt

karereDbSchemaTarget.target = karereDbSchema.cpp
karereDbSchemaTarget.depends = FORCE
karereDbSchemaTarget.commands = cmake -P $$MEGACHAT_BASE_PATH/src/genDbSchema.cmake
PRE_TARGETDEPS += karereDbSchema.cpp
QMAKE_EXTRA_TARGETS += karereDbSchemaTarget

DISTFILES += \
    $$PWD/../../src/dbSchema.sql
