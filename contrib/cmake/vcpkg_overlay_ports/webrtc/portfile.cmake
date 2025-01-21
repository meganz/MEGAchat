
# Chromium releases (Milestones) and associated WebRTC commits:
#    https://chromiumdash.appspot.com/releases?platform=Linux
#    https://chromiumdash.appspot.com/branches
#

if(EXISTS "${CURRENT_INSTALLED_DIR}/include/openssl/ssl.h")
	message(FATAL_ERROR "Can't build WebRTC if BoringSSL or OpenSSL is installed. Please remove BoringSSL or OpenSSL, and try to install WebRTC again. WebRTC will install headers and a .pc file to be found as OpenSSL.")
endif()

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Getting depot_tools
vcpkg_from_git(
    OUT_SOURCE_PATH DEPOT_TOOLS_PATH
    URL https://chromium.googlesource.com/chromium/tools/depot_tools.git
    REF 584e8366be0f67311eb5219180a0f400fdbea486
)

# Look for needed commands
find_program(GIT git REQUIRED)
find_program(FETCH fetch PATHS "${DEPOT_TOOLS_PATH}" REQUIRED)
find_program(GCLIENT gclient PATHS "${DEPOT_TOOLS_PATH}" REQUIRED)

# Define which webrtc config is needed depending on the target OS.
if (VCPKG_TARGET_IS_ANDROID)
    set(fetch_config webrtc_android)
elseif (VCPKG_TARGET_IS_IOS)
    set(fetch_config webrtc_ios)
else()
    set(fetch_config webrtc)
endif()

# Sources are not cached in VCPKG, so each configuration in a different folder makes it easier
# to at least reuse the already downloaded sources.
# That way, if we are building for 4 archs for Android, we only download the needed ~30GB once.
set(SOURCE_PATH "${CURRENT_BUILDTREES_DIR}/src/webrtc-${fetch_config}-${VERSION}")
set(WEBRTC_SOURCES_PATH "${SOURCE_PATH}/src")

if(NOT EXISTS ${SOURCE_PATH}/sources.ready)

    # Clean-up in case of not fully ready sources
    file(REMOVE_RECURSE "${SOURCE_PATH}" )
    file(MAKE_DIRECTORY "${SOURCE_PATH}")

    message(STATUS " * Getting ${fetch_config} and its dependencies. It will take a while...")

    vcpkg_execute_required_process(
        COMMAND ${FETCH} --nohook ${fetch_config}
        WORKING_DIRECTORY ${SOURCE_PATH}
        LOGNAME fetch-${PORT}-initial
    )

    message(STATUS " * Checking out revision branch-heads/${VERSION} for ${fetch_config}...")

    vcpkg_execute_required_process(
        COMMAND ${GIT} checkout branch-heads/${VERSION}
        WORKING_DIRECTORY ${WEBRTC_SOURCES_PATH}
        LOGNAME checkout-${PORT}-branch
    )

    vcpkg_execute_required_process(
        COMMAND ${GCLIENT} sync -D
        WORKING_DIRECTORY ${SOURCE_PATH}
        LOGNAME gclient-sync-${PORT}-branch
    )

    if (VCPKG_TARGET_IS_LINUX)
        vcpkg_apply_patches(
            SOURCE_PATH ${WEBRTC_SOURCES_PATH}
            PATCHES fix_include.patch
        )
    endif()

    # Mark sources as ready to build
    file(TOUCH ${SOURCE_PATH}/sources.ready)
else()
    message(STATUS "Using current sources at ${SOURCE_PATH}. Remove sources if you encounter building problems.")
endif()

# Parameters for "gn gen" command.
# All possible and currently set arameters can be listed with "gn args <out/directory> --list" command in a local build
set(is_component_build false) # Links dynamically if set to true
set(target_cpu "${VCPKG_TARGET_ARCHITECTURE}") # Target CPU architecture. Possible values: x86 x64 arm arm64 ...
set(use_custom_libcxx false) # Use in-tree libc++ instead of the system one when set to true.
set(treat_warnings_as_errors false)
set(fatal_linker_warnings false)
set(rtc_include_tests false)
set(libyuv_include_tests false)
set(rtc_disable_logging true) # Too verbose otherwise for regular development.
set(rtc_build_examples false)
set(rtc_build_tools false)
set(rtc_enable_protobuf false) # Enables the use of protocol buffers for recordings

if (VCPKG_TARGET_IS_ANDROID)
    set(target_os "android")
    if(DEFINED VCPKG_CMAKE_SYSTEM_VERSION)
        string(REPLACE "android-" "" android_api_level ${VCPKG_CMAKE_SYSTEM_VERSION}) # set android_api_level to "XY" from "android-XY"
    endif()
elseif (VCPKG_TARGET_IS_IOS)
    set(target_os "ios")
    if (DEFINED VCPKG_OSX_DEPLOYMENT_TARGET)
        set(ios_deployment_target "${VCPKG_OSX_DEPLOYMENT_TARGET}")
    endif()
    if(VCPKG_OSX_SYSROOT STREQUAL "iphonesimulator")
        set(target_environment "simulator")
    endif()
    set(ios_enable_code_signing false)
elseif (VCPKG_TARGET_IS_LINUX)
    set(is_clang false) # Set to true when compiling with the Clang compiler
    set(use_sysroot false) # Use in-tree sysroot if true
else()
    message(FATAL_ERROR "WebRTC port is only adapted for Linux, Android and iOS.")
endif()

# Append "gn gen" parameters which are not common for all platforms.
if (DEFINED target_os)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} target_os=\"${target_os}\"")
endif()
if(DEFINED is_clang)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} is_clang=${is_clang}")
endif()
if (DEFINED android_api_level)
    if (${target_cpu} STREQUAL arm64 OR ${target_cpu} STREQUAL x64)
        set(EXTRA_OPTIONS "${EXTRA_OPTIONS} android64_ndk_api_level=${android_api_level}")
    elseif (${target_cpu} STREQUAL arm OR ${target_cpu} STREQUAL x86)
        set(EXTRA_OPTIONS "${EXTRA_OPTIONS} android32_ndk_api_level=${android_api_level}")
    endif()
endif()
if(DEFINED ios_enable_code_signing)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} ios_enable_code_signing=${ios_enable_code_signing}")
endif()
if(DEFINED use_sysroot)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} use_sysroot=${use_sysroot}")
endif()
if(DEFINED ios_deployment_target)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} ios_deployment_target=\"${ios_deployment_target}\"")
endif()
if(DEFINED target_environment)
    set(EXTRA_OPTIONS "${EXTRA_OPTIONS} target_environment=\"${target_environment}\"")
endif()

if("enable-debug-log" IN_LIST FEATURES)
    set(rtc_disable_logging false)
endif()

message(STATUS " * Configuring and building...")

# gn scripts in WebRTC requires vpython3 tool which is available in the depot tools folder.
set(ENV{PATH} "${DEPOT_TOOLS_PATH}:$ENV{PATH}")

vcpkg_gn_configure(
    SOURCE_PATH "${WEBRTC_SOURCES_PATH}"
    OPTIONS "is_component_build=${is_component_build} target_cpu=\"${target_cpu}\" treat_warnings_as_errors=${treat_warnings_as_errors} fatal_linker_warnings=${fatal_linker_warnings} use_custom_libcxx=${use_custom_libcxx} libyuv_include_tests=${libyuv_include_tests} rtc_include_tests=${rtc_include_tests} rtc_build_examples=${rtc_build_examples} rtc_build_tools=${rtc_build_tools} rtc_enable_protobuf=${rtc_enable_protobuf} rtc_disable_logging=${rtc_disable_logging} ${EXTRA_OPTIONS}"
    OPTIONS_DEBUG "is_debug=true"
    OPTIONS_RELEASE "is_debug=false"
)

vcpkg_gn_install(
    SOURCE_PATH "${WEBRTC_SOURCES_PATH}"
    TARGETS :webrtc
)

# Install libwebrtc.jar for Android builds
if(VCPKG_TARGET_IS_ANDROID)
    if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
        file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/lib.java/sdk/android/libwebrtc.jar"
            DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
        )
    endif()
    if(VCPKG_BUILD_TYPE STREQUAL "release")
        file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/lib.java/sdk/android/libwebrtc.jar"
            DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
        )
    endif()
endif()

# Add symbolic links to impersonate OpenSSL with the internal BoringSSL.
if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
    file(CREATE_LINK ${CURRENT_PACKAGES_DIR}/lib/libwebrtc.a ${CURRENT_PACKAGES_DIR}/lib/libssl.a COPY_ON_ERROR SYMBOLIC)
    file(CREATE_LINK ${CURRENT_PACKAGES_DIR}/lib/libwebrtc.a ${CURRENT_PACKAGES_DIR}/lib/libcrypto.a COPY_ON_ERROR SYMBOLIC)
endif()
if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
    file(CREATE_LINK ${CURRENT_PACKAGES_DIR}/debug/lib/libwebrtc.a ${CURRENT_PACKAGES_DIR}/debug/lib/libssl.a COPY_ON_ERROR SYMBOLIC)
    file(CREATE_LINK ${CURRENT_PACKAGES_DIR}/debug/lib/libwebrtc.a ${CURRENT_PACKAGES_DIR}/debug/lib/libcrypto.a COPY_ON_ERROR SYMBOLIC)
endif()

# Install headers. Using COPY instead of INSTALL to reduce verbosity
message(STATUS " * Installing public headers...")
file(COPY "${WEBRTC_SOURCES_PATH}/api" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/rtc_base" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/third_party/abseil-cpp/absl" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/third_party/abseil-cpp/absl" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.inc")
file(COPY "${WEBRTC_SOURCES_PATH}/third_party/libyuv/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/modules" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/system_wrappers" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/common_video" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/call" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/media" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/video" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/p2p" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/logging" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${WEBRTC_SOURCES_PATH}/pc" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
if (VCPKG_TARGET_IS_ANDROID)
    file(COPY "${WEBRTC_SOURCES_PATH}/sdk" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
    file(COPY "${WEBRTC_SOURCES_PATH}/third_party/jni_zero" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party" FILES_MATCHING PATTERN "*.h")
endif()

file(COPY "${WEBRTC_SOURCES_PATH}/third_party/boringssl/src/include/openssl" DESTINATION "${CURRENT_PACKAGES_DIR}/include" FILES_MATCHING PATTERN "*.h")

# Clean empty directories
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/api/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/api/audio_codecs/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/api/test/metrics/proto"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/api/video_codecs/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/api/audio/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/rtc_base/java"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/absl/time/internal/cctz/testdata"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/absl/copts"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/pacing/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/utility/source"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_processing/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_processing/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_processing/transient/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_capture/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/codecs/isac/main/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/codecs/isac/fix/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/codecs/g711/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/codecs/ilbc/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/codecs/g722/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/neteq/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_coding/neteq/test/delay_tool"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/desktop_capture/win/cursor_test_data"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_mixer/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_device/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/audio_device/android"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/codecs/vp9/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/codecs/test/batch"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/codecs/vp8/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/codecs/h264/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/modules/video_coding/codecs/multiplex/test"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/system_wrappers/source"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/video/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/p2p/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/logging/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/pc/g3doc"
    "${CURRENT_PACKAGES_DIR}/include/${PORT}/pc/scenario_tests"
)

if (VCPKG_TARGET_IS_ANDROID)
    file(REMOVE_RECURSE
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/sdk/android/api"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/sdk/android/instrumentationtests"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/sdk/android/native_unittests"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/sdk/android/src/java"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/sdk/android/tests"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/codegen"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/java"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/__pycache__"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/sample"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/test"
        "${CURRENT_PACKAGES_DIR}/include/${PORT}/third_party/jni_zero/test_sample"
    )
endif()

message(STATUS " * Installing config and copyright files...")

# Manually configure variables for Config.cmake and *target.*.cmake files.
set(cmake_target_name ${PORT})
string(TOUPPER ${cmake_target_name} cmake_target_name_upper)
set(cmake_target_alias ${cmake_target_name_upper}::${cmake_target_name})

set(cmake_target_definitions
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE
    RTC_ENABLE_VP9
    WEBRTC_HAVE_SCTP
    WEBRTC_LIBRARY_IMPL
    ABSL_ALLOCATOR_NOTHROW=1
    LIBYUV_DISABLE_SME
    LIBYUV_DISABLE_LSX
    LIBYUV_DISABLE_LASX
    WEBRTC_POSIX
)

if (rtc_disable_logging)
    list(APPEND cmake_target_definitions
            RTC_DISABLE_LOGGING
        )
endif()

if(VCPKG_TARGET_IS_LINUX)
    list(APPEND cmake_target_definitions
        WEBRTC_LINUX
        LIBYUV_DISABLE_NEON
        LIBYUV_DISABLE_SVE
    )
    set(cmake_target_libs
        -lX11
        -lXfixes
        -lXdamage
        -lXrandr
        -lXtst
        -lXcomposite
        -lXext
    )
    set(pc_target_libs -lX11)
endif()

if(VCPKG_TARGET_IS_ANDROID)
    list(APPEND cmake_target_definitions
        WEBRTC_LINUX
        WEBRTC_ANDROID
    )
    if (${target_cpu} STREQUAL arm64)
        list(APPEND cmake_target_definitions
            WEBRTC_HAS_NEON
            WEBRTC_ARCH_ARM64
        )
    elseif (${target_cpu} STREQUAL arm)
        list(APPEND cmake_target_definitions
            WEBRTC_HAS_NEON
            LIBYUV_DISABLE_SVE
            WEBRTC_ARCH_ARM
            WEBRTC_ARCH_ARM_V7
        )
    elseif (${target_cpu} STREQUAL x86 OR ${target_cpu} STREQUAL x64)
        list(APPEND cmake_target_definitions
            LIBYUV_DISABLE_NEON
            LIBYUV_DISABLE_SVE
        )
    endif()
endif()

if(VCPKG_TARGET_IS_IOS)
    list(APPEND cmake_target_definitions
        WEBRTC_MAC
        WEBRTC_IOS
    )
    if (${target_cpu} STREQUAL arm64)
        list(APPEND cmake_target_definitions
            WEBRTC_HAS_NEON
            WEBRTC_ARCH_ARM64
        )
    elseif (${target_cpu} STREQUAL x64)
        list(APPEND cmake_target_definitions
            LIBYUV_DISABLE_NEON
            LIBYUV_DISABLE_SVE
        )
    endif()
endif()

list(JOIN cmake_target_definitions " " cmake_target_definitions) # INTERFACE_COMPILE_DEFINITIONS in the targets file requires a space separated list

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in
    "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}Config.cmake"
    INSTALL_DESTINATION share/${PORT}
    )

configure_file("${CMAKE_CURRENT_LIST_DIR}/${PORT}Targets.cmake.in" "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}Targets.cmake" @ONLY)

if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "release")
    set(cmake_build_type "release")
    set(cmake_install_dir "lib")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/${PORT}Targets-buildtype.cmake.in" "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}Targets-release.cmake" @ONLY)
endif()
if(NOT DEFINED VCPKG_BUILD_TYPE OR VCPKG_BUILD_TYPE STREQUAL "debug")
    set(cmake_build_type "debug")
    set(cmake_install_dir "debug/lib")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/${PORT}Targets-buildtype.cmake.in" "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}Targets-debug.cmake" @ONLY)
endif()

#TODO Replace the internal boringssl by the port in VCPKG and stop impersonating OpenSSL.
include("${CMAKE_CURRENT_LIST_DIR}/install-pc-files.cmake")

file(INSTALL "${WEBRTC_SOURCES_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
file(INSTALL "${WEBRTC_SOURCES_PATH}/PATENTS" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright-2)
