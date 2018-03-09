#-------------------------------------------------
#
# Project created by QtCreator 2018-02-05T17:49:14
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MEGAChatQt
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES +=

HEADERS +=

FORMS +=




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

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += svg

TARGET = megachat
TEMPLATE = app

DEFINES += LOG_TO_LOGGER


CONFIG += USE_LIBUV
CONFIG += USE_MEGAAPI
CONFIG += USE_MEDIAINFO
CONFIG += ENABLE_CHAT
CONFIG += USE_WEBRTC
DEFINES += ENABLE_CHAT

include(../../../bindings/qt/megachat.pri)


DEPENDPATH += examples/qtmegachatapi/
INCLUDEPATH += ../../../examples/qtmegachatapi/


SOURCES +=  ../../../examples/qtmegachatapi/MegaChatApplication.cpp \
            ../../../examples/qtmegachatapi/LoginDialog.cpp \
            ../../../examples/qtmegachatapi/MainWindow.cpp \
            ../../../examples/qtmegachatapi/chatItemWidget.cpp \
            ../../../examples/qtmegachatapi/chatWindow.cpp \
    ../../../examples/qtmegachatapi/contactItemWidget.cpp \
    ../../../examples/qtmegachatapi/uiSettings.cpp \
    ../../../examples/qtmegachatapi/chatSettings.cpp \
    ../../../examples/qtmegachatapi/megaLoggerApplication.cpp \
    ../../../examples/qtmegachatapi/chatMessage.cpp \
    ../../../examples/qtmegachatapi/callGui.cpp \
    ../../../examples/qtmegachatapi/callListener.cpp \
    ../../../examples/qtmegachatapi/remoteCallListener.cpp \
    ../../../examples/qtmegachatapi/localCallListener.cpp \
    ../../src/videoRenderer_Qt.cpp

HEADERS +=  ../../../examples/qtmegachatapi/MegaChatApplication.h \
            ../../../examples/qtmegachatapi/MainWindow.h \
            ../../../examples/qtmegachatapi/LoginDialog.h \
            ../../../examples/qtmegachatapi/chatItemWidget.h \
            ../../../examples/qtmegachatapi/chatWindow.h \
            ../../../examples/qtmegachatapi/widgetSubclass.h \
    ../../../examples/qtmegachatapi/contactItemWidget.h \
    ../../../examples/qtmegachatapi/uiSettings.h \
    ../../../examples/qtmegachatapi/chatSettings.h \
    ../../../examples/qtmegachatapi/megaLoggerApplication.h \
    ../../../examples/qtmegachatapi/chatMessage.h \
    ../../../examples/qtmegachatapi/callGui.h \
    ../../../examples/qtmegachatapi/callListener.h \
    ../../../examples/qtmegachatapi/remoteCallListener.h \
    ../../../examples/qtmegachatapi/localCallListener.h \
    ../../src/videoRenderer_Qt.h

FORMS +=    ../../../examples/qtmegachatapi/LoginDialog.ui \
            ../../../examples/qtmegachatapi/MainWindow.ui \
    ../../../examples/qtmegachatapi/chatWindow.ui \
    ../../../examples/qtmegachatapi/listItemWidget.ui \
    ../../../examples/qtmegachatapi/settingsDialog.ui \
    ../../../examples/qtmegachatapi/chatMessageWidget.ui \
    ../../../examples/qtmegachatapi/callGui.ui

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

RESOURCES += \
    ../../../examples/qtmegachatapi/res/resources.qrc
