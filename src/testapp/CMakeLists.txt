cmake_minimum_required(VERSION 2.8)
project(karere)
include (utils.cmake)
FIND_PACKAGE(Qt4 REQUIRED)

set(SRCS
	strophe.jingle.session.cpp
#    strophe.jingle.cpp
	webrtcAdapter.cpp
#    strophe.jingle.sdp.cpp
#    rtcSession.cpp
#  	test-promise.cpp
  	mainwindow.cpp
  	main.cpp
)

set(QT_FORMS mainwindow.ui)

QT4_WRAP_CPP(QT_GEN_MOC_SRCS mainwindow.h)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS})

add_definitions(-std=c++11)
add_subdirectory(../../webrtc-build/mybuild webrtc)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(karere ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
target_link_targets(karere webrtc_my)
target_link_libraries(karere ${QT_LIBRARIES})

