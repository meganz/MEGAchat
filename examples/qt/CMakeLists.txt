cmake_minimum_required(VERSION 2.8)
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
)

qt5_wrap_ui(ui_hdrs mainwindow.ui chat.ui chatMessage.ui clistitem.ui callGui.ui)
list(APPEND ui_hdrs widgetSubclass.h)

#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

add_definitions(${Qt5Widgets_DEFINITIONS} -D_DEBUG -fvisibility=hidden -Wall -DHAVE_KARERE_LOGGER)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../../src karere)
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(rtctestapp ${SRCS} ${ui_hdrs})
set_target_properties(rtctestapp PROPERTIES AUTOMOC TRUE)
target_link_libraries(rtctestapp
    ${Qt5Widgets_LIBRARIES}
    karere
    ${SYSLIBS}
)


