CONFIG += USE_LIBWEBSOCKETS
include(../../third-party/mega/bindings/qt/sdk.pri)

CONFIG += c++11

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
            net/libwsIO.h \
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
            waiter/libuvWaiter.h \
            waiter/libeventWaiter.h

RESOURCES += dbSchema.sql

DEFINES += USE_LIBWEBSOCKETS=1

CONFIG(qt) {
  SOURCES += ../bindings/qt/QTMegaChatEvent.cpp \
            ../bindings/qt/QTMegaChatListener.cpp \
            ../bindings/qt/QTMegaChatRoomListener.cpp \
            ../bindings/qt/QTMegaChatRequestListener.cpp \
            ../bindings/qt/QTMegaChatCallListener.cpp \
            ../bindings/qt/QTMegaChatVideoListener.cpp \
            ../bindings/qt/QTMegaChatNotificationListener.cpp
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

!release {
    DEFINES += DEBUG
}
else {
    DEFINES += NDEBUG
}

karereDbSchemaTarget.target = karereDbSchema.cpp
karereDbSchemaTarget.depends = FORCE
karereDbSchemaTarget.commands = cmake -P ../../src/genDbSchema.cmake
PRE_TARGETDEPS += karereDbSchema.cpp
QMAKE_EXTRA_TARGETS += karereDbSchemaTarget

DISTFILES +=
