cmake_minimum_required(VERSION 2.8)
project(testapp)

include (../utils.cmake)
FIND_PACKAGE(Qt4 REQUIRED)

set (optWebrtcCmakeBuild "~/webrtc-buildsys" CACHE PATH "Path to CMakeLists.txt file that puts together webrtc libs")
set (SRCS
     ../webrtcAdapter.cpp
     ../videoRenderer_Qt.cpp
     mainwindow.cpp
     main.cpp
     karereCommon.cpp
)

set(QT_FORMS mainwindow.ui)

QT4_WRAP_CPP(QT_GEN_MOC_SRCS mainwindow.h ../videoRenderer_Qt.h)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS} -D_DEBUG)

add_definitions(-std=c++11)
add_subdirectory("${optWebrtcCmakeBuild}" webrtc)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ..)

add_executable(testapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
target_link_targets(testapp webrtc_my)
target_link_libraries(testapp ${QT_LIBRARIES} pthread)

