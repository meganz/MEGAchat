#-------------------------------------------------
#
# Project created by QtCreator 2018-02-05T17:49:14
#
#-------------------------------------------------

# We assume libraw as a dependency for QtApp, because HAVE_LIBRAW is added to config.h
# if libraw is available in the system, although is not specified in the configure.
CONFIG += USE_LIBRAW

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += svg

include(../../../bindings/qt/megachat.pri)

TARGET = megachat
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
    ../../../examples/qtmegachatapi/chatGroupDialog.cpp \
    ../../../examples/qtmegachatapi/listItemController.cpp \
    ../../../examples/qtmegachatapi/SettingWindow.cpp \
    ../../../examples/qtmegachatapi/reaction.cpp \
    ../../../examples/qtmegachatapi/confirmAccount.cpp

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
    ../../../examples/qtmegachatapi/chatGroupDialog.h \
    ../../../examples/qtmegachatapi/listItemController.h \
    ../../../examples/qtmegachatapi/SettingWindow.h \
    ../../../examples/qtmegachatapi/reaction.h \
    ../../../examples/qtmegachatapi/confirmAccount.h

FORMS +=    ../../../examples/qtmegachatapi/LoginDialog.ui \
            ../../../examples/qtmegachatapi/MainWindow.ui \
    ../../../examples/qtmegachatapi/chatWindow.ui \
    ../../../examples/qtmegachatapi/listItemWidget.ui \
    ../../../examples/qtmegachatapi/settingsDialog.ui \
    ../../../examples/qtmegachatapi/chatMessageWidget.ui \
    ../../../examples/qtmegachatapi/chatGroupDialog.ui \
    ../../../examples/qtmegachatapi/SettingWindow.ui \
    ../../../examples/qtmegachatapi/reaction.ui \
    ../../../examples/qtmegachatapi/confirmAccount.ui

CONFIG(ENABLE_WERROR_COMPILATION) {
    # disable warnings emanating from Qt headers
    CONFIG += no_private_qt_headers_warning
    QMAKE_CXXFLAGS += -isystem $$[QT_INSTALL_HEADERS] \
                      -isystem $$[QT_INSTALL_HEADERS]/QtCore \
                      -isystem $$[QT_INSTALL_HEADERS]/QtGui \
                      -isystem $$[QT_INSTALL_HEADERS]/QtWidgets \
                      -isystem $$[QT_INSTALL_HEADERS]/QtSvg
}

CONFIG(USE_WEBRTC) {
    SOURCES +=  ../../src/videoRenderer_Qt.cpp \
                ../../../examples/qtmegachatapi/peerWidget.cpp \
                ../../../examples/qtmegachatapi/meetingView.cpp \
                ../../../examples/qtmegachatapi/meetingSession.cpp

    HEADERS +=  ../../src/videoRenderer_Qt.h \
                ../../../examples/qtmegachatapi/peerWidget.h \
                ../../../examples/qtmegachatapi/meetingView.h \
                ../../../examples/qtmegachatapi/meetingSession.h

    FORMS += ../../../examples/qtmegachatapi/callGui.ui
}

RESOURCES += \
    ../../../examples/qtmegachatapi/res/resources.qrc
