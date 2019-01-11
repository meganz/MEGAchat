CONFIG -= qt
TARGET = megachat_tests
DEPENDPATH += ../../../tests/sdk_test
INCLUDEPATH += ../../../tests/sdk_test
SOURCES +=  ../../../tests/sdk_test/sdk_test.cpp
HEADERS +=  ../../../tests/sdk_test/sdk_test.h

include(../../../bindings/qt/mega.pri)
include(../../../bindings/qt/megachat.pri)

macx {
    CONFIG += nofreeimage # there are symbols duplicated in libwebrtc.a. Discarded for the moment
}
