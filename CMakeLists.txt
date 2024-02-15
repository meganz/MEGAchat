# CMakeLists.txt file to build the MEGAchat library, tests and utilities.
#
# It can be used to build a standalone library or to be included via add_subdirectory.
#
# To include the project in your application use the following:
#    add_subdirectory(path/to/megachat)
#    target_link_libraries(<target> PRIVATE MEGA::CHATlib)
#

cmake_minimum_required(VERSION 3.18)

# Qt Creator configures VCPKG automatically. Disable it, we may want to use different tripplets, paths...
set(QT_CREATOR_SKIP_VCPKG_SETUP TRUE CACHE BOOL "")

## Modules location
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/contrib/cmake/modules) # Modules from MEGAchat
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/third-party/mega/contrib/cmake/modules) # Modules from MEGAsdk

## Configurable options ##
set(VCPKG_ROOT "" CACHE PATH "If set, it will build and use the VCPKG packages defined in the manifest file")
include(chatlib_options)

# If PROJECT_NAME is not set before project() we are the main project.
if(NOT PROJECT_NAME)
    message(STATUS "[CHATlib] is a top-level project. Install target, examples and tests will be enabled by default.")
    set(CHATLIB_STANDALONE 1)
    option(ENABLE_CHATLIB_QTAPP "Qt App example application is built if enabled" ON)
    option(ENABLE_CHATLIB_TESTS "Integration tests are built if enabled" ON)
    option(ENABLE_CHATLIB_WERROR "Enable warnings as errors" ON)
else()
    message(STATUS "[CHATlib] is building under project [${PROJECT_NAME}] Install target, examples and tests will not be enabled by default.")
    set(SDKLIB_STANDALONE 0)
    option(ENABLE_CHATLIB_QTAPP "Example application is built if enabled" OFF)
    option(ENABLE_CHATLIB_TESTS "Integration and unit tests are built if enabled" OFF)
    option(ENABLE_CHATLIB_WERROR "Enable warnings as errors" OFF)
endif()

if(NOT PROJECT_NAME)
    if(VCPKG_ROOT)
        # Include VCPKG management tools.
        include(vcpkg_management)
        include(chatlib_vcpkg_management) # For specific MEGAchat configurations
        list(APPEND vcpkg_overlay ${CMAKE_CURRENT_LIST_DIR}/third-party/mega/contrib/cmake) # MEGAsdk overlays
        process_vcpkg_libraries("${vcpkg_overlay}") # Choose and build libraries depending on the configured options.
    else()
        # For packages with no pkg-config in the system.
        list(APPEND CMAKE_MODULE_PATH third-party/mega/contrib/cmake/modules/packages)
        message(STATUS "Using system dependencies")
    endif()
endif()

project(CHATlib
    VERSION 0.0.1
    DESCRIPTION "MEGAchat Library"
    )

# In-source build not allowed
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source build is not allowed. Remove CMakeCache.txt and the CMakeFiles directory and set a new binary directory different than the source tree.")
endif()

message(STATUS "Building CHATlib v${PROJECT_VERSION}")

include(target_sources_conditional) # function to add files to the project without building them
include(target_platform_compile_options) # To add compile options depeding on the platform

## Load targets

# Load SDK project
add_subdirectory(third-party/mega)

# Load global CMake configuration for the project
include(chatlib_configuration)

# Load the MEGAchat library target
add_subdirectory(src)

# Load Qt bindings for MEGAchat
if(ENABLE_QT_BINDINGS)
    add_subdirectory(bindings/qt)
endif()

# Load Qt App example.
if(ENABLE_CHATLIB_QTAPP)
    add_subdirectory(examples/qtmegachatapi)
endif()
