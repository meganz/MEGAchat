cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

include (../src/utils.cmake)

project(webrtc_monolithic)


if (NOT WEBRTC_BUILD_TYPE)
    set(WEBRTC_BUILD_TYPE Release CACHE STRING "WebRTC build mode" FORCE)
endif()

if (NOT WEBRTC_BUILD_TYPE)
    message(FATAL_ERROR "WEBRTC_BUILD_TYPE must be already set")
endif()

set(webrtcRoot "~/webrtc-build/trunk" CACHE PATH "Path to the source&build tree of webrtc")
set(webrtcBuild "${webrtcRoot}/out/${WEBRTC_BUILD_TYPE}/obj")

set(optWebrtcShared 0 CACHE BOOL "Build webrtc as shared lib")
if (NOT APPLE)
set(WEBRTC_LIBS
    ${webrtcBuild}/talk/libjingle_peerconnection.a
    ${webrtcBuild}/talk/libjingle_p2p.a
    ${webrtcBuild}/talk/libjingle_media.a
    ${webrtcBuild}/webrtc/video_engine/libvideo_engine_core.a
    ${webrtcBuild}/webrtc/voice_engine/libvoice_engine.a
    ${webrtcBuild}/talk/libjingle_sound.a
    ${webrtcBuild}/talk/libjingle.a
    ${webrtcBuild}/webrtc/modules/libvideo_capture_module.a
    ${webrtcBuild}/webrtc/modules/libaudio_coding_module.a
    ${webrtcBuild}/webrtc/modules/libCNG.a
    ${webrtcBuild}/webrtc/modules/libG711.a
    ${webrtcBuild}/webrtc/modules/libG722.a
    ${webrtcBuild}/webrtc/modules/libiLBC.a
    ${webrtcBuild}/webrtc/modules/libiSAC.a
    ${webrtcBuild}/webrtc/modules/libiSACFix.a
    ${webrtcBuild}/webrtc/modules/libPCM16B.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_opus.a
    ${webrtcBuild}/webrtc/modules/libneteq.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_video_coding.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_i420.a
    ${webrtcBuild}/webrtc/modules/video_coding/utility/libvideo_coding_utility.a
    ${webrtcBuild}/webrtc/modules/video_coding/codecs/vp8/libwebrtc_vp8.a
    ${webrtcBuild}/webrtc/modules/libvideo_render_module.a
    ${webrtcBuild}/webrtc/modules/librtp_rtcp.a
    ${webrtcBuild}/webrtc/modules/libpaced_sender.a
    ${webrtcBuild}/webrtc/modules/libbitrate_controller.a
    ${webrtcBuild}/webrtc/modules/libvideo_processing.a
    ${webrtcBuild}/webrtc/modules/libaudio_conference_mixer.a
    ${webrtcBuild}/webrtc/modules/libaudio_processing.a
    ${webrtcBuild}/webrtc/modules/libaudioproc_debug_proto.a
    ${webrtcBuild}/webrtc/modules/libaudio_device.a
    ${webrtcBuild}/webrtc/modules/remote_bitrate_estimator/librbe_components.a
    ${webrtcBuild}/webrtc/modules/libremote_bitrate_estimator.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_utility.a
    ${webrtcBuild}/webrtc/modules/libmedia_file.a
    ${webrtcBuild}/webrtc/common_audio/libcommon_audio.a
    ${webrtcBuild}/webrtc/common_video/libcommon_video.a
#    ${webrtcBuild}/webrtc/build/libsanitizer_options.a
    ${webrtcBuild}/webrtc/libwebrtc.a
    ${webrtcBuild}/webrtc/system_wrappers/source/libfield_trial_default.a
    ${webrtcBuild}/webrtc/libwebrtc_common.a
    ${webrtcBuild}/webrtc/base/libwebrtc_base.a
    ${webrtcBuild}/webrtc/system_wrappers/source/libsystem_wrappers.a

#    ${webrtcBuild}/third_party/libvpx/libvpx_asm_offsets_vp8.a
#    ${webrtcBuild}/third_party/libvpx/libvpx_intrinsics_mmx.a
#    ${webrtcBuild}/third_party/libvpx/libvpx_intrinsics_ssse3.a
#    ${webrtcBuild}/third_party/libvpx/libvpx_intrinsics_sse2.a
#    ${webrtcBuild}/third_party/libvpx/libvpx.a
    ${CMAKE_CURRENT_BINARY_DIR}/libmyvpx.a

    ${webrtcBuild}/third_party/opus/libopus.a
    ${webrtcBuild}/../libyuv.a
    ${webrtcBuild}/third_party/libjpeg_turbo/libjpeg_turbo.a
    ${webrtcBuild}/third_party/usrsctp/libusrsctplib.a
    ${webrtcBuild}/third_party/protobuf/libprotobuf_lite.a
    ${webrtcBuild}/third_party/jsoncpp/libjsoncpp.a
    ${webrtcBuild}/third_party/libsrtp/libsrtp.a
)
else() #APPLE
    file(GLOB WEBRTC_LIBS "${webrtcBuild}/../*.a")
endif()

set(WEBRTC_LIBS_X86
    ${webrtcBuild}/webrtc/modules/libvideo_processing_sse2.a
    ${webrtcBuild}/webrtc/common_audio/libcommon_audio_sse2.a
    ${webrtcBuild}/webrtc/modules/libaudio_processing_sse2.a
)
set(WEBRTC_LIBS_LINUX
    ${webrtcBuild}/net/third_party/nss/libcrssl.a
)
if (NOT APPLE) #apple build just globs all .a files, so we dont need to specially handle this
    set(WEBRTC_LIBS_ARM
        ${webrtcBuild}/webrtc/modules/libisac_neon.a
        ${webrtcBuild}/webrtc/modules/libaudio_processing_neon.a
        ${webrtcBuild}/webrtc/common_audio/libcommon_audio_neon.a
        ${webrtcBuild}/webrtc/modules/audio_processing/lib_core_neon_offsets.a
        ${webrtcBuild}/../libyuv_neon.a
    )
endif()

if (ANDROID)
    add_library(cpufeatures "${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c")
endif()

set(WEBRTC_LIBS_ANDROID
   ${webrtcBuild}/webrtc/system_wrappers/source/libcpu_features_android.a
   ${webrtcBuild}/third_party/icu/libicuuc.a
   ${webrtcBuild}/third_party/icu/libicudata.a
)

set(WEBRTC_DEFS -DV8_DEPRECATION_WARNINGS -DUSE_LIBJPEG_TURBO=1 -DCHROMIUM_BUILD
#the following defines are not fiund in any header, so maybe htey can be removed
 -DENABLE_WEBRTC=1 -DENABLE_CONFIGURATION_POLICY -DENABLE_AUTOFILL_DIALOG=1
-DENABLE_MANAGED_USERS=1
)

set(WEBRTC_DEFS_DESKTOP
    -DEXPAT_RELATIVE_PATH -DFEATURE_ENABLE_VOICEMAIL -DGTEST_RELATIVE_PATH
    -DJSONCPP_RELATIVE_PATH -DLOGGING=1 -DSRTP_RELATIVE_PATH
    -DFEATURE_ENABLE_SSL -DFEATURE_ENABLE_PSTN -DHAVE_SRTP     -DHAVE_SCTP
    -DHAVE_WEBRTC_VIDEO -DHAVE_WEBRTC_VOICE -DUSE_WEBRTC_DEV_BRANCH #so far IOS has all those
    -DENABLE_ONE_CLICK_SIGNIN -DENABLE_REMOTING=1  -DENABLE_PEPPER_CDMS
    -DENABLE_NOTIFICATIONS -DENABLE_TASK_MANAGER=1      -DENABLE_EXTENSIONS=1
    -DENABLE_PLUGINS=1 -DENABLE_PLUGIN_INSTALLATION=1 -DENABLE_SESSION_SERVICE=1
    -DENABLE_THEMES=1 -DENABLE_BACKGROUND=1 -DENABLE_GOOGLE_NOW=1
    -DENABLE_EGLIMAGE=1 #also on windows?
    -DENABLE_FULL_PRINTING=1 -DENABLE_PRINTING=1 -DENABLE_SPELLCHECK=1
    -DENABLE_CAPTIVE_PORTAL_DETECTION=1 -DENABLE_APP_LIST=1
    -DENABLE_SETTINGS_APP=1 -DENABLE_SERVICE_DISCOVERY=1 -DLIBPEERCONNECTION_LIB=1
    -D_REENTRANT
)

set(WEBRTC_DEFS_LINUX
    ${WEBRTC_DEFS_DESKTOP}
    -DUI_COMPOSITOR_IMAGE_TRANSPORT -DUSE_AURA=1 -DUSE_CAIRO=1 -DUSE_GLIB=1
    -DUSE_DEFAULT_RENDER_THEME=1  -DUSE_X11=1 -DUSE_CLIPBOARD_AURAX11=1 -DUSE_XI2_MT=2
    -DENABLE_INPUT_SPEECH -DUSE_UDEV -DCLD_VERSION=2 -DENABLE_MDNS=1
    -DLINUX -DWEBRTC_POSIX=1 -DHASH_NAMESPACE=__gnu_cxx -DPOSIX -DDISABLE_DYNAMIC_CAST
    -DSSL_USE_NSS -DHAVE_NSS_SSL_H -DSSL_USE_NSS_RNG -DUSE_NSS=1
    -DDYNAMIC_ANNOTATIONS_ENABLED=1 -DWTF_USE_DYNAMIC_ANNOTATIONS=1
    -DBLINK_SCALE_FILTERS_AT_RECORD_TIME -D_FILE_OFFSET_BITS=64 -DTOOLKIT_VIEWS=1
)

set(WEBRTC_DEFS_APPLE
    -DWEBRTC_MAC -DCARBON_DEPRECATED=YES
    -DHASH_NAMESPACE=__gnu_cxx -DPOSIX -DWEBRTC_POSIX
    -DDISABLE_DYNAMIC_CAST -DXML_STATIC
    -DDYNAMIC_ANNOTATIONS_ENABLED=0
    -DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY
    -DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE
    -DCR_CLANG_REVISION=214024
)

set(WEBRTC_DEFS_MACOS
     ${WEBRTC_DEFS_DESKTOP}
     ${WEBRTC_DEFS_APPLE}
    -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0
    -DENABLE_PRE_SYNC_BACKUP
    -DENABLE_HIDPI=1
    -DCLD_VERSION=2 -DCLD2_DATA_SOURCE=static
    -DENABLE_WIFI_BOOTSTRAPPING=1
    -DOSX
)

set(WEBRTC_DEFS_IOS
    ${WEBRTC_DEFS_DESKTOP}
    ${WEBRTC_DEFS_APPLE}
    -DDISABLE_NACL -DCR_CLANG_REVISION=214024
    -DCLD_VERSION=1 -DENABLE_SPELLCHECK=1 -DDISABLE_FTP_SUPPORT=1
    -DIOS -DWEBRTC_IOS
    -D_REENTRANT
    -DNDEBUG -DNVALGRIND
    -DNS_BLOCK_ASSERTIONS=1
)

set(WEBRTC_DEFS_ANDROID #arm
    -D_FILE_OFFSET_BITS=64 -DNO_TCMALLOC
    -DDISABLE_NACL -DCHROMIUM_BUILD -DUSE_LIBJPEG_TURBO=1
    -DUSE_PROPRIETARY_CODECS -DENABLE_BROWSER_CDMS
    -DENABLE_CONFIGURATION_POLICY
    -DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY
    -DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE -DENABLE_EGLIMAGE=1
    -DENABLE_AUTOFILL_DIALOG=1 -DCLD_VERSION=1 -DENABLE_PRINTING=1
    -DENABLE_MANAGED_USERS=1 -DVIDEO_HOLE=1 -DWEBRTC_RESTRICT_LOGGING
    -DWEBRTC_MODULE_UTILITY_VIDEO -DWEBRTC_ARCH_ARM -DWEBRTC_ARCH_ARM_V7
    -DWEBRTC_DETECT_ARM_NEON -DWEBRTC_POSIX -DWEBRTC_LINUX -DWEBRTC_ANDROID
    -DWEBRTC_ANDROID_OPENSLES -DGTEST_HAS_POSIX_RE=0 -DGTEST_LANG_CXX11=0
    -DU_USING_ICU_NAMESPACE=0 -DU_ENABLE_DYLOAD=0 -DU_STATIC_IMPLEMENTATION
    -DUSE_OPENSSL=1 -DUSE_OPENSSL_CERTS=1 -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS -DANDROID -D__GNU_SOURCE=1 -DUSE_STLPORT=1
    -D_STLP_USE_PTR_SPECIALIZATIONS=1 '-DCHROME_BUILD_ID=""'
    -DHAVE_SYS_UIO_H -DDYNAMIC_ANNOTATIONS_ENABLED=1
    -DWTF_USE_DYNAMIC_ANNOTATIONS=1
)


set(WEBRTC_INCLUDES
    ${webrtcBuild}
    ${webrtcBuild}/gen
    ${webrtcRoot}
    ${webrtcRoot}/webrtc
    ${webrtcRoot}/third_party
    ${webrtcRoot}/third_party/jsoncpp/overrides/include
    ${webrtcRoot}/third_party/jsoncpp/source/include
    ${webrtcRoot}/third_party/libyuv/include
    ${webrtcRoot}/icu/source/common
    ${webrtcRoot}/net/third_party/nss/ssl
)

set(WEBRTC_SYSINCLUDES_LINUX
    /usr/include/glib-2.0
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
    /usr/include/gtk-2.0
    /usr/lib/x86_64-linux-gnu/gtk-2.0/include
    /usr/include/atk-1.0
    /usr/include/cairo
    /usr/include/gdk-pixbuf-2.0
    /usr/include/pango-1.0
    /usr/include/gio-unix-2.0/
    /usr/include/pixman-1
    /usr/include/freetype2
    /usr/include/libpng12
    /usr/include/nss
    /usr/include/nspr
)

set(WEBRTC_SYSINCLUCES_ANDROID)

set(WEBRTC_CFLAGS
    -fstack-protector --param=ssp-buffer-size=4
    -fno-strict-aliasing -Wno-unused-parameter
    -Wno-missing-field-initializers -pipe -fPIC
)
if (CMAKE_COMPILER_ID STREQUAL "GNU")
    list(APPEND WEBRTC_CLAGS -Wno-unused-local-typedefs)
endif()

set(WEBRTC_CXXFLAGS
    -fno-rtti -fno-exceptions -fno-threadsafe-statics
    -Wno-deprecated -std=gnu++11 -Wno-narrowing -Wno-literal-suffix
)

set(WEBRTC_CFLAGS_ANDROID #arm
    -Wno-strict-overflow -fno-builtin-cos
    -fno-builtin-sin -fno-builtin-cosf -fno-builtin-sinf -march=armv7-a
    -mtune=generic-armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp -mthumb
    -fno-tree-sra -fno-partial-inlining -fno-early-inlining
    -fno-tree-copy-prop -fno-tree-loop-optimize -fno-move-loop-invariants
    -fno-caller-saves -Wno-psabi -mthumb-interwork -ffunction-sections
    -funwind-tables -g -fstack-protector -fno-short-enums -finline-limit=64
    -Wa,--noexecstack
    -Os -g -fdata-sections -ffunction-sections -fomit-frame-pointer
    -funwind-tables
)

set(WEBRTC_CXXFLAGS_ANDROID -Wsign-compare -Wnon-virtual-dtor -Woverloaded-virtual -Wno-abi)

set(WEBRTC_FLAGS_APPLE
    -gdwarf-2 -Wnewline-eof -Wendif-labels
    -Wno-selector-type-mismatch -Wheader-hygiene -Wno-char-subscripts
    -Wno-unneeded-internal-declaration -Wno-covered-switch-default
    -Wstring-conversion -Wno-c++11-narrowing -Wno-deprecated-register
)

set(WEBRTC_FLAGS_MACOS
    ${WEBRTC_FLAGS_APPLE}
    -Werror -mmacosx-version-min=10.6 -arch x86_64
)

set(WEBRTC_CFLAGS_APPLE
    ${WEBRTC_FLAGS_APPLE}
    -std=c99 -fno-slp-vectorize -fcolor-diagnostics
)
set(WEBRTC_CFLAGS_MACOS ${WEBRTC_CFLAGS_APPLE})
set(WEBRTC_CFLAGS_IOS ${WEBRTC_CFLAGS_APPLE})

set(WEBRTC_CXXFLAGS_APPLE
    ${WEBRTC_FLAGS_APPLE}
    -fno-slp-vectorize -fcolor-diagnostics
    -fobjc-call-cxx-cdtors #this applies only for obj-cxx, but cmake does not have flags specially for obj-c++
)

set(WEBRTC_CXXFLAGS_MACOS
    ${WEBRTC_CXXFLAGS_APPLE} -fno-strict-aliasing
)
set(WEBRTC_CXXFLAGS_IOS
    ${WEBRTC_CXXFLAGS_APPLE} -fno-threadsafe-statics
)

set(WEBRTC_LDFLAGS)

set(WEBRTC_LDFLAGS_LINUX
    -Wl,-z,now -Wl,-z,relro -Wl,--fatal-warnings
    -Wl,-z,noexecstack -fPIC -Wl,--disable-new-dtags
)

set(WEBRTC_LDFLAGS_ANDROID
    -Wl,-z,now -Wl,-z,relro -Wl,--fatal-warnings
    -Wl,-z,noexecstack -fPIC
    -fuse-ld=gold -nostdlib -Wl,--no-undefined
    -Wl,--icf=safe
    -Wl,-shared,-Bsymbolic
    -Wl,--warn-shared-textrel -Wl,-O1 -Wl,--as-needed
)
set(WEBRTC_LDFLAGS_APPLE
    -Wl,-search_paths_first -Wl,-ObjC
    -Wl,-dead_strip
)

set(WEBRTC_LDFLAGS_MACOS ${WEBRTC_LDFLAGS_APPLE} -arch x86_64)
set(WEBRTC_LDFLAGS_IOS ${WEBRTC_LDFLAGS_APPLE})

set(WEBRTC_SYSLIBS)
set(WEBRTC_SYSLIBS_ANDROID cpufeatures log GLESv2 OpenSLES crypto ssl m dl)

set(WEBRTC_SYSLIBS_LINUX
    nss3 nssutil3 smime3 plds4 plc4 nspr4
    crypto expat pthread Xext X11 m dl
)

set(WEBRTC_SYSLIBS_APPLE
    "-framework AVFoundation" "-framework AudioToolbox"
    "-framework Foundation" "-framework Security"
    "-framework SystemConfiguration" "-framework CoreVideo"
    "-framework CoreMedia"
)

set(WEBRTC_SYSLIBS_MACOS
    ${WEBRTC_SYSLIBS_APPLE}
    "-framework OpenGL" "-framework WebKit" "-framework Cocoa"
    "-framework IOKit" "-framework CoreAudio"
    "-framework QTKit" #still needed by libjingle_media.a for device connect notification etc.
)
set(WEBRTC_SYSLIBS_IOS
    ${WEBRTC_SYSLIBS_APPLE}
    sqlite3 "-framework GLKit" "-framework OpenGLES"
   "-framework UIKit" "-framework QuartzCore"
)

macro(appendPlatformStuff pname)
    list(APPEND WEBRTC_DEFS ${WEBRTC_DEFS_${pname}})
    list(APPEND WEBRTC_INCLUDES ${WEBRTC_INCLUDES_${pname}})
    list(APPEND WEBRTC_INCLUDES ${WEBRTC_SYSINCLUDES_${pname}})
    list(APPEND WEBRTC_CFLAGS ${WEBRTC_CFLAGS_${pname}})
    list(APPEND WEBRTC_CXXFLAGS ${WEBRTC_CXXFLAGS_${pname}})
    list(APPEND WEBRTC_LDFLAGS ${WEBRTC_LDFLAGS_${pname}})
    list(APPEND WEBRTC_LIBS ${WEBRTC_LIBS_${pname}})
    list(APPEND WEBRTC_SYSLIBS ${WEBRTC_SYSLIBS_${pname}})

    listToSpacedString(WEBRTC_CFLAGS)
    listToSpacedString(WEBRTC_CXXFLAGS)
    listToSpacedString(WEBRTC_LDFLAGS)
endmacro()

macro(appendArchStuff aname)
    list(APPEND WEBRTC_LIBS ${WEBRTC_LIBS_${aname}})
endmacro()

if ("${WEBRTC_BUILD_TYPE}" STREQUAL "Debug")
    list(APPEND WEBRTC_DEFS  -D_DEBUG)
else()
    list(APPEND WEBRTC_DEFS -DNDEBUG -DNVALGRIND)
endif()

if (ANDROID)
    message(STATUS "Building rtcModule for arm Android")
    appendArchStuff(ARM)
    appendPlatformStuff(ANDROID)
elseif(APPLE_IOS)
    message(STATUS "Building rtcModule for arm iOS")
    appendArchStuff(ARM)
    appendPlatformStuff(IOS)
elseif(APPLE) #macosx
    message(STATUS "Building rtcModule for x86 MacOSX")
    # appendArchStuff(X86)
    appendPlatformStuff(MACOS)
elseif(UNIX) #linux
    message(STATUS "Building rtcModule for x86 Linux")
    appendArchStuff(X86)
    appendPlatformStuff(LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

if (NOT APPLE)
    #put together all object files of vpx in a single .a because there are circular dependencies between the original libs
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libmyvpx.a
        COMMAND ar crs ${CMAKE_CURRENT_BINARY_DIR}/libmyvpx.a
            `find ${webrtcBuild}/third_party/libvpx/ -name '*.o'`
            `find ${webrtcBuild}/../gen/third_party/libvpx -name '*.o'`
        DEPENDS "${webrtcBuild}/third_party/libvpx/source/"
    )
    set(myvpx libmyvpx.a)
endif()

set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${WEBRTC_LDFLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${WEBRTC_LDFLAGS}")


if(optWebrtcShared)
    add_library(webrtc_my SHARED dummy.cpp ${myvpx})
else()
    add_library(webrtc_my dummy.cpp ${myvpx})
endif()
target_link_libraries(webrtc_my ${WEBRTC_LIBS} ${WEBRTC_SYSLIBS})

set(WEBRTC_SYSLIBS ${WEBRTC_SYSLIBS} CACHE INTERNAL "")
set(WEBRTC_LDFLAGS "-fno-rtti" CACHE INTERNAL "")
set(WEBRTC_DEFS ${WEBRTC_DEFS} CACHE INTERNAL "")
set(WEBRTC_INCLUDES ${WEBRTC_INCLUDES} CACHE INTERNAL "")

