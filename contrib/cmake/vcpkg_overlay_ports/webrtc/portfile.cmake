
# Chromium releases (Milestones) and associated WebRTC commits:
#    https://chromiumdash.appspot.com/releases?platform=Linux
#    https://chromiumdash.appspot.com/branches
#
# Commits for dependencies can be found in the DEPS file in the root of the WebRTC repository.
#

if(EXISTS "${CURRENT_INSTALLED_DIR}/include/openssl/ssl.h")
	message(FATAL_ERROR "Can't build WebRTC if BoringSSL or OpenSSL is installed. Please remove BoringSSL or OpenSSL, and try to install WebRTC again. WebRTC will install headers and a .pc file to be found as OpenSSL.")
endif()

# Checkout and move to DESTINATION. SOURCE_PATH is the base folder for DESTINATION
function(vcpkg_sub_from_git)
    set(oneValueArgs DESTINATION URL REF)
    cmake_parse_arguments(sub_from_git "" "${oneValueArgs}" "" ${ARGN})

    set(directory "${SOURCE_PATH}/${sub_from_git_DESTINATION}")

    if(EXISTS "${directory}")
        message(STATUS "Using existing ${directory}")
        return()
    endif()

    vcpkg_from_git(
        OUT_SOURCE_PATH sub_from_git_current_dest
        URL ${sub_from_git_URL}
        REF ${sub_from_git_REF}
    )

    # move into SOURCE_PATH
    file(RENAME "${sub_from_git_current_dest}" "${directory}")

endfunction()


vcpkg_from_git(
    OUT_SOURCE_PATH SOURCE_PATH
    URL https://webrtc.googlesource.com/src
    REF 93081d594f7efff72958a79251f53731b99e902b # Release M108
)

message(STATUS " * Working on submodules and other dependencies, it may take a while...")

vcpkg_sub_from_git(
    DESTINATION build
    URL https://chromium.googlesource.com/chromium/src/build
    REF 1c4f38fd4f534d78b72cefc376a03b3e8b486e7c
)
file(WRITE ${SOURCE_PATH}/build/config/gclient_args.gni "generate_location_tags = true")

vcpkg_sub_from_git(
    DESTINATION third_party
    URL https://chromium.googlesource.com/chromium/src/third_party
    REF 28a4580f804c4fc4279ecf10100a409811030235
)

vcpkg_sub_from_git(
    DESTINATION testing
    URL https://chromium.googlesource.com/chromium/src/testing
    REF 6f2362298838e8789b09282cf198c8582f9c4555
)

vcpkg_sub_from_git(
    DESTINATION tools
    URL https://chromium.googlesource.com/chromium/src/tools
    REF d97453670f86a8cc5050802a4a49083c5db3b39a
)

vcpkg_sub_from_git(
    DESTINATION third_party/libyuv
    URL https://chromium.googlesource.com/libyuv/libyuv
    REF 00950840d1c9bcbb3eb6ebc5aac5793e71166c8b
)

vcpkg_sub_from_git(
    DESTINATION third_party/libsrtp
    URL https://chromium.googlesource.com/chromium/deps/libsrtp
    REF 5b7c744eb8310250ccc534f3f86a2015b3887a0a
)

vcpkg_sub_from_git(
    DESTINATION third_party/catapult
    URL https://chromium.googlesource.com/catapult
    REF 4793433248183dd073e608f655204d4acfdc7193
)

vcpkg_sub_from_git(
    DESTINATION third_party/nasm
    URL https://chromium.googlesource.com/chromium/deps/nasm
    REF 9215e8e1d0fe474ffd3e16c1a07a0f97089e6224
)

vcpkg_sub_from_git(
    DESTINATION third_party/boringssl/src
    URL https://boringssl.googlesource.com/boringssl
    REF 1ee71185a2322dc354bee5e5a0abfb1810a27dc6
)

vcpkg_sub_from_git(
    DESTINATION third_party/libjpeg_turbo
    URL https://chromium.googlesource.com/chromium/deps/libjpeg_turbo
    REF ed683925e4897a84b3bffc5c1414c85b97a129a3
)

vcpkg_sub_from_git(
    DESTINATION third_party/icu
    URL https://chromium.googlesource.com/chromium/deps/icu
    REF 20f8ac695af59b6c830def7d4e95bfeb13dd7be5
)

vcpkg_sub_from_git(
    DESTINATION third_party/crc32c/src
    URL https://chromium.googlesource.com/external/github.com/google/crc32c
    REF fa5ade41ee480003d9c5af6f43567ba22e4e17e6
)

vcpkg_sub_from_git(
    DESTINATION third_party/libvpx/source/libvpx
    URL https://chromium.googlesource.com/webm/libvpx
    REF 9d6d0624d7943a09cc0be9df1a7402522989ac1a
)

vcpkg_sub_from_git(
    DESTINATION third_party/libaom/source/libaom
    URL https://aomedia.googlesource.com/aom
    REF 7f32eb35ff2589369f095388701e3dfc4d6a9381
)

vcpkg_sub_from_git(
    DESTINATION third_party/dav1d/libdav1d
    URL https://chromium.googlesource.com/external/github.com/videolan/dav1d
    REF 87f9a81cd770e49394a45deca7a3df41243de00b
)

vcpkg_sub_from_git(
    DESTINATION third_party/jsoncpp/source
    URL https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp
    REF 42e892d96e47b1f6e29844cc705e148ec4856448
)

vcpkg_sub_from_git(
    DESTINATION third_party/grpc/src
    URL https://chromium.googlesource.com/external/github.com/grpc/grpc
    REF dd77c67217b10ffeaf766e25eb8b46d2d59de4ff
)

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Parameters for "gn gen" command.
# Parameters can be listed with "gn args <out/directory> --list" command in a local build
set(is_component_build false) # Links dynamically if set to true
set(target_cpu "${VCPKG_TARGET_ARCHITECTURE}") # Target CPU architecture. Possible values: x86 x64 arm arm64 ...
set(use_custom_libcxx false) # Use in-tree libc++ instead of the system one.
set(is_clang false) # Set to true when compiling with the Clang compiler
set(use_sysroot false) # # Use in-tree sysroot
set(treat_warnings_as_errors false)
set(fatal_linker_warnings false)
set(rtc_include_tests false)
set(libyuv_include_tests false)

message(STATUS " * Configuring and building...")

vcpkg_gn_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS "is_component_build=${is_component_build} target_cpu=\"${target_cpu}\" use_custom_libcxx=${use_custom_libcxx} is_clang=${is_clang} use_sysroot=${use_sysroot} treat_warnings_as_errors=${treat_warnings_as_errors} fatal_linker_warnings=${fatal_linker_warnings} rtc_include_tests=${rtc_include_tests} libyuv_include_tests=${libyuv_include_tests}"
    OPTIONS_DEBUG "is_debug=true"
    OPTIONS_RELEASE "is_debug=false"
)

vcpkg_gn_install(
    SOURCE_PATH "${SOURCE_PATH}"
    TARGETS :webrtc
)

# Install headers. Using COPY instead of INSTALL to reduce verbosity
message(STATUS " * Installing public headers...")
file(COPY "${SOURCE_PATH}/api" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/rtc_base" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/third_party/abseil-cpp/absl" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/third_party/libyuv/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/modules" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/system_wrappers" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/common_video" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/call" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/media" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/video" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/p2p" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/logging" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")
file(COPY "${SOURCE_PATH}/pc" DESTINATION "${CURRENT_PACKAGES_DIR}/include/${PORT}" FILES_MATCHING PATTERN "*.h")

file(COPY "${SOURCE_PATH}/third_party/boringssl/src/include/openssl" DESTINATION "${CURRENT_PACKAGES_DIR}/include" FILES_MATCHING PATTERN "*.h")

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

message(STATUS " * Installing config and copyright files...")

# Manually configure variables for Config.cmake and *target.*.cmake files.
set(cmake_target_name ${PORT})
string(TOUPPER ${cmake_target_name} cmake_target_name_upper)
set(cmake_target_alias ${cmake_target_name_upper}::${cmake_target_name})

set(cmake_target_definitions
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

if(VCPKG_TARGET_IS_LINUX)
    list(APPEND cmake_target_definitions
        WEBRTC_POSIX
        WEBRTC_LINUX
    )
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

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
file(INSTALL "${SOURCE_PATH}/PATENTS" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright-2)
