
add_library(CHATQtBindings)
add_library(MEGA::CHATQtBindings ALIAS CHATQtBindings)

target_sources(CHATQtBindings
    PRIVATE
    QTMegaChatCallListener.cpp
    QTMegaChatCallListener.h
    QTMegaChatEvent.cpp
    QTMegaChatEvent.h
    QTMegaChatListener.cpp
    QTMegaChatListener.h
    QTMegaChatNodeHistoryListener.cpp
    QTMegaChatNodeHistoryListener.h
    QTMegaChatNotificationListener.cpp
    QTMegaChatNotificationListener.h
    QTMegaChatRequestListener.cpp
    QTMegaChatRequestListener.h
    QTMegaChatRoomListener.cpp
    QTMegaChatRoomListener.h
    QTMegaChatSchedMeetListener.cpp
    QTMegaChatSchedMeetListener.h
    QTMegaChatVideoListener.cpp
    QTMegaChatVideoListener.h
)

# Activate the meta-object code generator for Qt.
set_target_properties(CHATQtBindings PROPERTIES AUTOMOC ON)

## Required for the Qt generated files, to be able to include other headers
target_include_directories(CHATQtBindings
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For the top level projects.
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # For the external projects.
)

find_package(Qt5 REQUIRED COMPONENTS Core)
message(STATUS "Building the MEGAchat Qt Bindings with Qt v${Qt5_VERSION} from ${Qt5_DIR}")

target_link_libraries(CHATQtBindings
    PRIVATE
    Qt5::Core
    MEGA::CHATlib
)

## Adjust compilation flags for warnings and errors ##
target_platform_compile_options(
    TARGET CHATQtBindings
    UNIX $<$<CONFIG:Debug>:-ggdb3> -Wall -Wextra -Wconversion -Wno-unused-parameter
)

if(ENABLE_CHATLIB_WERROR)
    target_platform_compile_options(
        TARGET CHATQtBindings
        UNIX  $<$<CONFIG:Debug>: -Werror
                                 -Wno-error=deprecated-declarations> # Kept as a warning, do not promote to error.
        APPLE $<$<CONFIG:Debug>: -Wno-inconsistent-missing-override>
    )
endif()
