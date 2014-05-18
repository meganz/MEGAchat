cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

include (utils.cmake)

project(webrtc_monolithic)

set(webrtcRoot "${CMAKE_CURRENT_SOURCE_DIR}/../trunk" CACHE PATH "Path to the source&build tree of webrtc")
set(optShared 0 CACHE BOOL "Build webrtc as shared lib")

set(webrtcBuild "${webrtcRoot}/out/${CMAKE_BUILD_TYPE}/obj")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build mode")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug RelWithDebInfo MinSizeRel)
endif()

#set(LIBDIRS
#    ${webrtcBuild}/net/third_party/nss
#    ${webrtcBuild}/third_party/jsoncpp
#    ${webrtcBuild}/talk
#    ${webrtcBuild}/..
#    ${webrtcBuild}/third_party/libjpeg_turbo
#    ${webrtcBuild}/third_party/usrsctp
#    ${webrtcBuild}/webrtc/modules
#    ${webrtcBuild}/webrtc/common_audio
#    ${webrtcBuild}/webrtc/system_wrappers/source
#    ${webrtcBuild}/third_party/opus
#    ${webrtcBuild}/webrtc/common_video
#    ${webrtcBuild}/webrtc/modules/video_coding/utility
#    ${webrtcBuild}/webrtc/modules/video_coding/codecs/vp8
#    ${webrtcBuild}/third_party/libvpx
#    ${webrtcBuild}/webrtc/video_engine
#    ${webrtcBuild}/webrtc/voice_engine
#    ${webrtcBuild}/webrtc/modules/remote_bitrate_estimator
#    ${webrtcBuild}/third_party/protobuf
#    ${webrtcBuild}/third_party/libsrtp
#)

set(LIBS

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
    ${webrtcBuild}/webrtc/modules/libNetEq4.a
    ${webrtcBuild}/webrtc/modules/libmedia_file.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_video_coding.a
    ${webrtcBuild}/webrtc/modules/libwebrtc_i420.a
    ${webrtcBuild}/webrtc/modules/video_coding/utility/libvideo_coding_utility.a
    ${webrtcBuild}/webrtc/modules/video_coding/codecs/vp8/libwebrtc_vp8.a
    ${webrtcBuild}/webrtc/modules/libvideo_render_module.a
    ${webrtcBuild}/webrtc/modules/librtp_rtcp.a
    ${webrtcBuild}/webrtc/modules/libpaced_sender.a
    ${webrtcBuild}/webrtc/modules/libbitrate_controller.a
    ${webrtcBuild}/webrtc/modules/libvideo_processing.a
    ${webrtcBuild}/webrtc/modules/libvideo_processing_sse2.a
    ${webrtcBuild}/webrtc/modules/libaudio_conference_mixer.a
    ${webrtcBuild}/webrtc/modules/libaudio_processing.a
    ${webrtcBuild}/webrtc/modules/libaudioproc_debug_proto.a
    ${webrtcBuild}/webrtc/modules/libaudio_processing_sse2.a
    ${webrtcBuild}/webrtc/modules/libaudio_device.a
    ${webrtcBuild}/webrtc/modules/remote_bitrate_estimator/librbe_components.a

    ${webrtcBuild}/webrtc/modules/libremote_bitrate_estimator.a

    ${webrtcBuild}/webrtc/modules/libwebrtc_utility.a

    ${webrtcBuild}/webrtc/common_audio/libcommon_audio.a
    ${webrtcBuild}/webrtc/common_audio/libcommon_audio_sse2.a
    ${webrtcBuild}/webrtc/common_video/libcommon_video.a

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
    ${webrtcBuild}/net/third_party/nss/libcrssl.a
    ${webrtcBuild}/third_party/jsoncpp/libjsoncpp.a
    ${webrtcBuild}/net/third_party/nss/libcrssl.a

    ${webrtcBuild}/third_party/libsrtp/libsrtp.a

)


set(DEFS
    -DV8_DEPRECATION_WARNINGS -DBLINK_SCALE_FILTERS_AT_RECORD_TIME
    -DEXPAT_RELATIVE_PATH -DFEATURE_ENABLE_VOICEMAIL -DGTEST_RELATIVE_PATH
    -DJSONCPP_RELATIVE_PATH -DLOGGING=1 -DSRTP_RELATIVE_PATH
    -DFEATURE_ENABLE_SSL -DFEATURE_ENABLE_PSTN -DHAVE_SRTP
    -DHAVE_WEBRTC_VIDEO -DHAVE_WEBRTC_VOICE -DUSE_WEBRTC_DEV_BRANCH
    -D_FILE_OFFSET_BITS=64 -DCHROMIUM_BUILD -DTOOLKIT_VIEWS=1
    -DUI_COMPOSITOR_IMAGE_TRANSPORT -DUSE_AURA=1 -DUSE_CAIRO=1 -DUSE_GLIB=1
    -DUSE_DEFAULT_RENDER_THEME=1 -DUSE_LIBJPEG_TURBO=1 -DUSE_X11=1
    -DUSE_CLIPBOARD_AURAX11=1 -DENABLE_ONE_CLICK_SIGNIN -DUSE_XI2_MT=2
    -DENABLE_REMOTING=1 -DENABLE_WEBRTC=1 -DENABLE_PEPPER_CDMS
    -DENABLE_CONFIGURATION_POLICY -DENABLE_INPUT_SPEECH
    -DENABLE_NOTIFICATIONS -DUSE_UDEV -DENABLE_EGLIMAGE=1
    -DENABLE_TASK_MANAGER=1 -DENABLE_EXTENSIONS=1
    -DENABLE_PLUGIN_INSTALLATION=1 -DENABLE_PLUGINS=1
    -DENABLE_SESSION_SERVICE=1 -DENABLE_THEMES=1 -DENABLE_AUTOFILL_DIALOG=1
    -DENABLE_BACKGROUND=1 -DENABLE_GOOGLE_NOW=1 -DCLD_VERSION=2
    -DENABLE_FULL_PRINTING=1 -DENABLE_PRINTING=1 -DENABLE_SPELLCHECK=1
    -DENABLE_CAPTIVE_PORTAL_DETECTION=1 -DENABLE_APP_LIST=1
    -DENABLE_SETTINGS_APP=1 -DENABLE_MANAGED_USERS=1 -DENABLE_MDNS=1
    -DENABLE_SERVICE_DISCOVERY=1 -DLIBPEERCONNECTION_LIB=1 -DLINUX
    -DHAVE_SCTP -DHASH_NAMESPACE=__gnu_cxx -DPOSIX -DDISABLE_DYNAMIC_CAST
    -D_REENTRANT -DSSL_USE_NSS -DHAVE_NSS_SSL_H -DSSL_USE_NSS_RNG
    -DUSE_NSS=1 -DDYNAMIC_ANNOTATIONS_ENABLED=1
    -DWTF_USE_DYNAMIC_ANNOTATIONS=1
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    list(APPEND DEFS  -D_DEBUG -D_GLIBCXX_DEBUG=1)
endif()

set(INCLUDES
    ${webrtcRoot}
    ${webrtcRoot}/third_party
    ${webrtcRoot}/third_party/webrtc
    ${webrtcRoot}/../../net/third_party/nss/ssl
    ${webrtcRoot}/third_party/jsoncpp/overrides/include
    ${webrtcRoot}/third_party/jsoncpp/source/include
)

set(CFLAGS
    -fstack-protector --param=ssp-buffer-size=4 -pthread
    -fno-exceptions -fno-strict-aliasing -Wno-unused-parameter
    -Wno-missing-field-initializers -fvisibility=hidden -pipe -fPIC
)

set(CCXXFLAGS
    -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden
    -Wno-deprecated
)

set(SYSINCLUDES
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



set(LDFLAGS
    -Wl,-z,now -Wl,-z,relro -Wl,--fatal-warnings -pthread
    -Wl,-z,noexecstack -fPIC
    -Wl,--disable-new-dtags
    #-Wl,--whole-archive
    -m64
    #-Wl,--icf=none
)

set(SYSLIBS
    nss3 nssutil3 smime3 plds4 plc4 nspr4 -ldl
    crypto expat m
)

EXECUTE_PROCESS(COMMAND find "${webrtcBuild}/third_party/libvpx/source" "-name" "*.o"
            OUTPUT_VARIABLE vpxobjs_raw1)
EXECUTE_PROCESS(COMMAND find "${webrtcBuild}/../gen/third_party/libvpx" "-name" "*.o"
            OUTPUT_VARIABLE vpxobjs_raw2)

set(vpxobjs_raw "${vpxobjs_raw1} ${vpxobjs_raw2}")
string(REGEX REPLACE "[\n\r]" " " vpxobjs "${vpxobjs_raw}")
separate_arguments(vpxobjs)

add_definitions(${DEFS})
include_directories(${INCLUDES} ${SYSINCLUDES})
add_definitions(${CFLAGS} ${CCXXFLAGS})

add_library(webrtc_my SHARED dummy.c)
add_custom_target(myvpx
 COMMAND ar crs ${CMAKE_CURRENT_BINARY_DIR}/libmyvpx.a ${vpxobjs} VERBATIM
)

target_link_libraries(webrtc_my ${LIBS} ${SYSLIBS})
add_dependencies(webrtc_my myvpx)

listToSpacedString(LDFLAGS)
editTargetProp(webrtc_my LINK_FLAGS ADD ${LDFLAGS})
targetExportDefines(webrtc_my ${DEFS})
targetExportFullpathIncludes(webrtc_my ${INCLUDES} ${SYSINCLUDES})
targetExportCompileFlags(webrtc_my -fno-rtti)

