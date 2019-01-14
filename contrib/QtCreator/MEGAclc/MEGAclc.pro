
CONFIG -= qt
CONFIG += USE_AUTOCOMPLETE
CONFIG += console
CONFIG -= app_bundle
CONFIG += USE_CONSOLE
CONFIG += MEGACLC

include(../../../bindings/qt/megachat.pri)

TARGET = megaclc

LIBS += -lstdc++fs -lreadline -ltermcap
DEPENDPATH += ../../../examples/megaclc
INCLUDEPATH += ../../../examples/megaclc
INCLUDEPATH += ../../../third-party/mega/sdk_build/install/include
QMAKE_LIBDIR += ../../../third-party/mega/sdk_build/install/lib

SOURCES +=  ../../../examples/megaclc/megaclc.cpp

