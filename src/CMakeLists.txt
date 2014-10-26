cmake_minimum_required(VERSION 2.8)
project(karere)
include (utils.cmake)
FIND_PACKAGE(Qt4 REQUIRED)
set (optWebrtcCmakeBuild "../../webrtc-buildsys" CACHE PATH "Path to CMakeLists.txt file that puts together webrtc libs")
set (optStrophePath "../../strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")
set (SRCS
    strophe.jingle.session.cpp
    strophe.disco.cpp
    strophe.jingle.cpp
     webrtcAdapter.cpp
     videoRenderer_Qt.cpp
    strophe.jingle.sdp.cpp
#    rtcSession.cpp
#    test-promise.cpp
     testapp/mainwindow.cpp
     testapp/main.cpp
)

set(QT_FORMS testapp/mainwindow.ui)

QT4_WRAP_CPP(QT_GEN_MOC_SRCS testapp/mainwindow.h videoRenderer_Qt.h)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS} -D_DEBUG)

add_definitions(-std=c++11)
add_subdirectory("${optWebrtcCmakeBuild}" webrtc)
add_subdirectory("${optStrophePath}" strophe)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${optStrophePath} ${optStrophePath}/include)

add_executable(karere ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
target_link_targets(karere webrtc_my)
target_link_libraries(karere strophe ${QT_LIBRARIES} pthread)

