cmake_minimum_required(VERSION 3.0)
project(rtctestapp)

FIND_PACKAGE(Qt5Widgets REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
     mainwindow.cpp
     chatWindow.cpp
     callGui.cpp
     main.cpp
     ../../src/videoRenderer_Qt.cpp
     ../../src/karereCommon.cpp
#need to include also headers with Q_OBJECTs so that automoc knows to parse them
     widgetSubclass.h
#==
     res/resources.qrc
)

add_definitions(-D_DEBUG -fvisibility=hidden -Wall -DHAVE_KARERE_LOGGER)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../../src karere)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(rtctestapp ${SRCS})
set_target_properties(rtctestapp PROPERTIES AUTOMOC TRUE AUTOUIC TRUE AUTORCC TRUE)
target_link_libraries(rtctestapp
    Qt5::Widgets
    karere
    ${SYSLIBS}
)

