cmake_minimum_required(VERSION 2.8)
project(objctestapp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

#need to ad headers explicitly so that the xcode project sees them
set (SRCS
     AppDelegate.mm
     mainWindow.mm
     mainWindow.h
     main.mm
     ../../src/videoRenderer_objc.mm
     ../../src/videoRenderer_objc.h
     ../../src/karereCommon.cpp
)
set(RESOURCES
    gui.storyboard
    LaunchScreen.xib
    Entitlements.plist
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wno-unused-local-typedefs)
endif()

add_subdirectory(../../src karere)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${KARERE_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SYSLIBS)
if (CLANG_STDLIB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=lib${CLANG_STDLIB}")
    set(SYSLIBS ${CLANG_STDLIB})
endif()

set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS Entitlements.plist)

add_executable(objctestapp ${SRCS} ${RESOURCES})
set_target_properties(objctestapp PROPERTIES RESOURCE "${RESOURCES}")
 set_target_properties(objctestapp PROPERTIES
     MACOSX_BUNDLE_INFO_STRING "${PROJECT_NAME}, Copyright 2015 Mega LTD."
     MACOSX_BUNDLE_ICON_FILE ""
     MACOSX_BUNDLE_GUI_IDENTIFIER "${PROJECT_NAME}"
     MACOSX_BUNDLE_LONG_VERSION_STRING "${MACOSX_BUNDLE_INFO_STRING}"
     MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
     MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_STRING}"
     MACOSX_BUNDLE_BUNDLE_VERSION "${VERSION_STRING}"
     MACOSX_BUNDLE_COPYRIGHT "Copyright 2015, MEGA Ltd. All Rights Reserved."
 )

set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in")

target_link_libraries(objctestapp
    karere
    ${SYSLIBS}
    "-framework CoreGraphics"
)



