cmake_minimum_required(VERSION 2.8)
project(rtctestapp)
include (../utils.cmake)

set (optStrophePath "../../strophe-native" CACHE PATH "Path to our custom strophe (mstrophe) lib")


FIND_PACKAGE(Qt4 REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
find_package(LibEvent REQUIRED)

set (SRCS
     ../videoRenderer_Qt.cpp
     mainwindow.cpp
     main.cpp
     ../strophe.disco.cpp
     ../DummyCrypto.cpp
)

set(QT_FORMS mainwindow.ui)

QT4_WRAP_CPP(QT_GEN_MOC_SRCS mainwindow.h ../videoRenderer_Qt.h)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
ADD_DEFINITIONS(${QT_DEFINITIONS} -D_DEBUG -fvisibility=hidden)

add_definitions(-std=c++11)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}
    ${optStrophePath}
    ${optStrophePath}/include
    ${LIBEVENT_INCLUDE_DIR}
)

add_executable(rtctestapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
target_link_libraries(
rtctestapp
/home/loki/karere-native/build/librtcModule.so
${optStrophePath}/build/libstrophe.so
${QT_LIBRARIES}
pthread
${LIBEVENT_LIB}
event_pthreads
)


