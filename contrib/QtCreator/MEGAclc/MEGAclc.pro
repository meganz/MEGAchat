
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
SOURCES +=  ../../../examples/megaclc/mclc_autocompletion.cpp
SOURCES +=  ../../../examples/megaclc/mclc_chat_and_call_actions.cpp
SOURCES +=  ../../../examples/megaclc/mclc_enums_to_string.cpp
SOURCES +=  ../../../examples/megaclc/mclc_logging.cpp
SOURCES +=  ../../../examples/megaclc/mclc_commands.cpp
SOURCES +=  ../../../examples/megaclc/mclc_globals.cpp
SOURCES +=  ../../../examples/megaclc/mclc_listeners.cpp
SOURCES +=  ../../../examples/megaclc/mclc_general_utils.cpp
SOURCES +=  ../../../examples/megaclc/mclc_prompt.cpp
SOURCES +=  ../../../examples/megaclc/mclc_reports.cpp
