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
     ../base/cservices.cpp
     ../base/cservices-strophe.cpp
     ../base/cservices-dns.cpp
     ../base/cservices-http.cpp
     base32.c
)

set(QT_FORMS mainwindow.ui)
QT4_WRAP_UI(QT_GEN_FORMS_HEADERS ${QT_FORMS})
#QT4_ADD_RESOURCES(helloworld_RESOURCES_RCC ${helloworld_RESOURCES})

INCLUDE(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS} -D_DEBUG -fvisibility=hidden -Wall)

#list(APPEND CMAKE_CXX_FLAGS -std=c++11)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
add_subdirectory(.. rtcModule)
get_property(STROPHE_INCLUDES GLOBAL PROPERTY STROPHE_INCLUDES)

set (optMegaSdkPath "/home/loki/megasdk" CACHE PATH "Mega SDK path")
if (WIN32)
  if (WINPHONE8)
    set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/wp8")
  else()
    set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/win32")
  endif()
else()
  set(MEGASDK_PLATFORM_INCLUDES "${optMegaSdkPath}/include/mega/posix")
endif()

set(MEGASDK_INCLUDES "${optMegaSdkPath}/include" "${MEGASDK_PLATFORM_INCLUDES}")
set(MEGASDK_LIB "${optMegaSdkPath}/src/.libs/libmega.so")

include_directories(
    ..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${STROPHE_INCLUDES}
    ${MEGASDK_INCLUDES}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
add_executable(rtctestapp ${SRCS} ${QT_GEN_MOC_SRCS} ${QT_GEN_FORMS_HEADERS})
set_target_properties(rtctestapp PROPERTIES AUTOMOC TRUE)
target_link_libraries(rtctestapp
    ${QT_LIBRARIES}
    rtcModule
    ${MEGASDK_LIB}
)


