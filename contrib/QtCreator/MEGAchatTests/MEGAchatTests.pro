CONFIG -= qt

include(../../../bindings/qt/megachat.pri)

TARGET = megachat_tests
DEPENDPATH += ../../../tests/sdk_test
INCLUDEPATH += ../../../tests/sdk_test $$MEGASDK_BASE_PATH/tests
SOURCES +=  ../../../tests/sdk_test/sdk_test.cpp \
            $$MEGASDK_BASE_PATH/tests/gtest_common.cpp
HEADERS +=  ../../../tests/sdk_test/sdk_test.h \
            $$MEGASDK_BASE_PATH/tests/gtest_common.h

macx {
    CONFIG += nofreeimage # there are symbols duplicated in libwebrtc.a. Discarded for the moment
}

vcpkg {
    debug:LIBS += -lgmockd -lgtestd
    !debug:LIBS += -lgmock -lgtest
}
else {
    # Jenkins should have gtest installed in the path below
    GTEST_PATH_JENKINS=/opt/gtest/gtest-1.10.0
    exists($$GTEST_PATH_JENKINS/include/gtest/gtest.h) {
        INCLUDEPATH += $$GTEST_PATH_JENKINS/include
    }
    exists($$GTEST_PATH_JENKINS/lib/libgtest.a) {
        LIBS += -L$$GTEST_PATH_JENKINS/lib
    }

    LIBS += -lgmock -lgtest
}

# Copy the logo.png file to the test build dir needed for one test
MEGACHAT_ROOT_DIR = $$PWD/../../..
QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$MEGACHAT_ROOT_DIR/tests/sdk_test/logo.png) $$quote($$OUT_PWD) $$escape_expand(\\n\\t)

