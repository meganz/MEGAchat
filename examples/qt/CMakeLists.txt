cmake_minimum_required(VERSION 3.0)
project(rtctestapp)

FIND_PACKAGE(Qt5Widgets REQUIRED)

set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

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

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(-D_DEBUG -fvisibility=hidden -Wall -Wno-unused-local-typedef -DHAVE_KARERE_LOGGER)
    if (optAsanMode AND ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")) # defined by Karere
        add_definitions(-fsanitize=${optAsanMode} -fno-omit-frame-pointer) #enable ASAN
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=${optAsanMode}")
    endif()
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
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

set_target_properties(rtctestapp PROPERTIES
 MACOSX_RPATH ON
 INSTALL_RPATH "@executable_path/deps"
)

INSTALL(TARGETS rtctestapp
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    )

INSTALL(CODE "
   include(BundleUtilities)
   fixup_bundle(rtctestapp \"\" \"\")
   " COMPONENT Runtime)

set(CPACK_BINARY_DRAGNDROP ON)
include(CPack)
