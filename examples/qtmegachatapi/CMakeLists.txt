# 
# MEGAchat Qt App example
# 

add_executable(QtApp)

set(QTAPP_HEADERS
    chatGroupDialog.h
    chatItemWidget.h
    chatMessage.h
    chatSettings.h
    chatWindow.h
    confirmAccount.h
    contactItemWidget.h
    listItemController.h
    LoginDialog.h
    MainWindow.h
    MegaChatApplication.h
    megaLoggerApplication.h
    reaction.h
    SettingWindow.h
    uiSettings.h
    widgetSubclass.h
)

set(QTAPP_SOURCES
    chatGroupDialog.cpp
    chatItemWidget.cpp
    chatMessage.cpp
    chatSettings.cpp
    chatWindow.cpp
    confirmAccount.cpp
    contactItemWidget.cpp
    listItemController.cpp
    LoginDialog.cpp
    MainWindow.cpp
    MegaChatApplication.cpp
    megaLoggerApplication.cpp
    reaction.cpp
    SettingWindow.cpp
    uiSettings.cpp
)

set(QTAPP_UI
    callGui.ui
    chatGroupDialog.ui
    chatMessageWidget.ui
    chatWindow.ui
    confirmAccount.ui
    listItemWidget.ui
    LoginDialog.ui
    MainWindow.ui
    reaction.ui
    settingsDialog.ui
    SettingWindow.ui
)

set(QTAPP_RESOURCES
    res/resources.qrc
)

target_sources(QtApp
    PRIVATE
    ${QTAPP_UI}
    ${QTAPP_HEADERS}
    ${QTAPP_SOURCES}
    ${QTAPP_RESOURCES}
)

target_sources_conditional(QtApp
    FLAG USE_WEBRTC
    PRIVATE
    peerWidget.h
    meetingSession.h
    meetingView.h
    
    peerWidget.cpp
    meetingSession.cpp    
    meetingView.cpp
)

# Activate properties for Qt code
set_target_properties(QtApp
    PROPERTIES
    AUTOMOC ON # Activates the meta-object code generator for Qt.
    AUTOUIC ON # Activates the User Interface Compiler generator for Qt.
)

# Required for the Qt generated files, to be able to include other headers in the current path
target_include_directories(QtApp
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Load and link needed libraries for the CHATlib target
find_package(Qt5 REQUIRED COMPONENTS Widgets)
target_link_libraries(QtApp
    PRIVATE
    Qt5::Widgets
    MEGA::CHATlib # MEGAchat library (And MEGAsdk library)
)

)

    )
endif()
