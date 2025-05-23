find_package(Java REQUIRED)
find_package(SWIG REQUIRED)
include(UseSWIG)

# Set the output directory for generated Java files
set(JAVA_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bindings/java/nz/mega/sdk)

# Define the interface file and its properties
set(SWIG_INTERFACE_FILE ${CMAKE_CURRENT_LIST_DIR}/../megachatapi.i)
set_source_files_properties(${SWIG_INTERFACE_FILE} PROPERTIES
    CPLUSPLUS ON
)

# Set SWIG flags for generating Java code
if(ENABLE_SYNC)
    list(APPEND ADDITIONAL_SWIG_DEFINES -DENABLE_SYNC)
endif()

if(USE_LIBUV)
    list(APPEND ADDITIONAL_SWIG_DEFINES -DHAVE_LIBUV)
endif()

if(NOT USE_WEBRTC)
    list(APPEND ADDITIONAL_SWIG_DEFINES -DKARERE_DISABLE_WEBRTC)
endif()

set(CMAKE_SWIG_FLAGS -c++ -package "nz.mega.sdk" ${ADDITIONAL_SWIG_DEFINES} -I${CMAKE_CURRENT_LIST_DIR}/../../src -I${CMAKE_CURRENT_LIST_DIR}/../../third-party/mega/include)

# Generate java binding files
swig_add_library(MEGAchatJavaBindings
    TYPE USE_BUILD_SHARED_LIBS
    LANGUAGE java
    SOURCES ${SWIG_INTERFACE_FILE}
    OUTPUT_DIR ${JAVA_OUTPUT_DIR}
)

# For Android we do not need JNI
if(ANDROID)
    set(JNI_INCLUDE_DIRS
        ${ANDROID_NDK_HOME}/sysroot/usr/include
        ${ANDROID_NDK_HOME}/sysroot/usr/include/${ANDROID_ABI}
    )
else()
    find_package(JNI REQUIRED)
endif()

target_include_directories(MEGAchatJavaBindings
    PRIVATE
    ${Java_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
    ${JAVA_OUTPUT_DIR}
)

target_link_libraries(MEGAchatJavaBindings
    PRIVATE
    MEGA::CHATlib
    SDKJavaBindings
)

# Compile Java code
add_custom_command(TARGET MEGAchatJavaBindings POST_BUILD
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${JAVA_OUTPUT_DIR} -cp ${JAVA_OUTPUT_DIR} ${JAVA_OUTPUT_DIR}/*.java
    COMMENT "Compiling Java classes..."
)
