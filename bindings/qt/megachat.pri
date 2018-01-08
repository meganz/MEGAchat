MEGASDK_BASE_PATH = $$PWD/../../src

VPATH += $$MEGASDK_BASE_PATH
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
            waiter/libuvWaiter.cpp \
            ../bindings/qt/QTMegaChatEvent.cpp \
            ../bindings/qt/QTMegaChatListener.cpp \
            ../bindings/qt/QTMegaChatRoomListener.cpp \
            ../bindings/qt/QTMegaChatRequestListener.cpp

HEADERS  += asyncTest-framework.h \
            buffer.h \
            chatd.h \
            contactList.h \
            karereCommon.h \
            messageBus.h \
            serverListProviderForwards.h \
            textModule.h \
            videoRenderer_objc.h \
            asyncTest.h \
            chatClient.h \
            chatdICrypto.h \
            db.h \
            karereId.h \
            presenced.h \
            serverListProvider.h \
            textModuleTypeConfig.h \
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
            chatRoom.h \
            IGui.h \
            megachatapi_impl.h \
            sdkApi.h \
            strophe.disco.h \
            userAttrCache.h \
            ../bindings/qt/QTMegaChatEvent.h \
            ../bindings/qt/QTMegaChatListener.h \
            ../bindings/qt/QTMegaChatRoomListener.h \
            ../bindings/qt/QTMegaChatRequestListener.h

DEFINES += USE_LIBWEBSOCKETS=1 KARERE_DISABLE_WEBRTC=1 SVC_DISABLE_STROPHE

INCLUDEPATH += $$MEGASDK_BASE_PATH
INCLUDEPATH += $$MEGASDK_BASE_PATH/base
INCLUDEPATH += $$MEGASDK_BASE_PATH/rtcModule
INCLUDEPATH += $$MEGASDK_BASE_PATH/strongvelope
INCLUDEPATH += $$MEGASDK_BASE_PATH/../third-party
INCLUDEPATH += $$MEGASDK_BASE_PATH/../bindings/qt
INCLUDEPATH += $$MEGASDK_BASE_PATH/../bindings/qt/3rdparty/include

LIBS += -L$$MEGASDK_BASE_PATH/../bindings/qt/3rdparty/lib -lwebsockets -luv

!release {
    DEFINES += DEBUG
}
else {
    DEFINES += NDEBUG
}
