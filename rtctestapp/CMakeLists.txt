cmake_minimum_required(VERSION 2.8)
project(rtctestapp)

FIND_PACKAGE(Qt4 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
     mainwindow.cpp
     main.cpp
     ../src/videoRenderer_Qt.cpp
     ../src/karereCommon.cpp
)

set(QT_FORMS mainwindow.ui)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS} -D_DEBUG -fvisibility=hidden -Wall)
if (CMAKE_COMPILER_ID STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../src chatclient)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)
get_property(MEGASDK_INCLUDES GLOBAL PROPERTY MEGASDK_INCLUDES)

include_directories(
    ../src
    ../src/base
    ../src/rtcModule
    ../third-party
    ${CMAKE_CURRENT_BINARY_DIR}
    ${STROPHE_INCLUDES}
    ${MEGASDK_INCLUDES}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

add_executable(rtctestapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
set_target_properties(rtctestapp PROPERTIES AUTOMOC TRUE)
target_link_libraries(rtctestapp
    ${QT_LIBRARIES}
    chatclient
    ${SYSLIBS}
)


