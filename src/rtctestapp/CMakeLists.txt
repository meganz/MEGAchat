cmake_minimum_required(VERSION 2.8)
project(rtctestapp)
include (../utils.cmake)

FIND_PACKAGE(Qt4 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set (SRCS
     ../videoRenderer_Qt.cpp
     mainwindow.cpp
     main.cpp
     ../strophe.disco.cpp
     ../DummyCrypto.cpp
     ../base/services.cpp
     ../base/services-strophe.cpp
)

set(QT_FORMS mainwindow.ui)

QT4_WRAP_CPP(QT_GEN_MOC_SRCS mainwindow.h ../videoRenderer_Qt.h)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS} -D_DEBUG -fvisibility=hidden)

#list(APPEND CMAKE_CXX_FLAGS -std=c++11)

add_subdirectory(.. rtcModule)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)
include_directories(
    ..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${STROPHE_INCLUDES}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
add_executable(rtctestapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})

target_link_libraries(rtctestapp
    ${QT_LIBRARIES}
    rtcModule
)


