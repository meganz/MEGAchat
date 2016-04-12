cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0015 NEW)

include (../src/utils.cmake)

project(webrtc_monolithic)


if (NOT WEBRTC_BUILD_TYPE)
    set(WEBRTC_BUILD_TYPE Release CACHE STRING "WebRTC build mode" FORCE)
    set_property(CACHE WEBRTC_BUILD_TYPE PROPERTY STRINGS Release Debug)
endif()

if (NOT WEBRTC_BUILD_TYPE)
    message(FATAL_ERROR "WEBRTC_BUILD_TYPE must be already set")
endif()

set(webrtcRoot "~/webrtc-build/src" CACHE PATH "Path to the source&build tree of webrtc")
set(webrtcBuild "${webrtcRoot}/out/${WEBRTC_BUILD_TYPE}/obj")

set(optWebrtcShared 0 CACHE BOOL "Build webrtc as shared lib")

file(GLOB_RECURSE WEBRTC_LIBS "${webrtcBuild}/../*${CMAKE_STATIC_LIBRARY_SUFFIX}")
if (NOT APPLE)
    list(REMOVE_ITEM WEBRTC_LIBS "${webrtcBuild}/../obj/webrtc/modules/libvideo_capture${CMAKE_STATIC_LIBRARY_SUFFIX}") # The external capture interface
else()
    list(REMOVE_ITEM WEBRTC_LIBS "${webrtcBuild}/../libvideo_capture${CMAKE_STATIC_LIBRARY_SUFFIX}") # The external capture interface
endif()

set(WEBRTC_DEFS -DV8_DEPRECATION_WARNINGS -DUSE_LIBJPEG_TURBO=1 -DCHROMIUM_BUILD
 -DENABLE_WEBRTC=1 -DENABLE_CONFIGURATION_POLICY -DENABLE_AUTOFILL_DIALOG=1
-DENABLE_MANAGED_USERS=1
)

set(WEBRTC_DEFS_DESKTOP
    -DEXPAT_RELATIVE_PATH -DFEATURE_ENABLE_VOICEMAIL -DGTEST_RELATIVE_PATH
    -DJSONCPP_RELATIVE_PATH -DLOGGING=1 -DSRTP_RELATIVE_PATH
    -DFEATURE_ENABLE_SSL -DFEATURE_ENABLE_PSTN -DHAVE_SRTP     -DHAVE_SCTP
    -DHAVE_WEBRTC_VIDEO -DHAVE_WEBRTC_VOICE -DUSE_WEBRTC_DEV_BRANCH
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

set(WEBRTC_DEFS_MACOS
     ${WEBRTC_DEFS_DESKTOP}
    -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORE=0
    -DCR_CLANG_REVISION=214024
    -DENABLE_PRE_SYNC_BACKUP
    -DENABLE_HIDPI=1
    -DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY
    -DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE
#new defs
    -DDONT_EMBED_BUILD_METADATA
    -DENABLE_BASIC_PRINTING=1
    -DENABLE_PRINT_PREVIEW=1
    -DENABLE_SUPERVISED_USERS=1
    -DV8_USE_EXTERNAL_STARTUP_DATA
    -DUSE_LIBPCI=1 -DUSE_OPENSSL=1
    -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
    -DDYNAMIC_ANNOTATIONS_ENABLED=0
#    -D_FORTIFY_SOURCE=2 #conflicts with ASAN, which defines it internally
#===
    -DCLD_VERSION=2 -DCLD2_DATA_SOURCE=static
    -DENABLE_WIFI_BOOTSTRAPPING=1
    -DOSX -DWEBRTC_MAC -DCARBON_DEPRECATED=YES
    -DHASH_NAMESPACE=__gnu_cxx -DPOSIX -DWEBRTC_POSIX
    -DDISABLE_DYNAMIC_CAST -DXML_STATIC
    -DDYNAMIC_ANNOTATIONS_ENABLED=0
)

set(WEBRTC_DEFS_ANDROID #arm
    -D_FILE_OFFSET_BITS=64 -DNO_TCMALLOC
    -DDISABLE_NACL -DUSE_PROPRIETARY_CODECS -DENABLE_BROWSER_CDMS
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

set(WEBRTC_DEFS_IOS
    -DWEBRTC_IOS -DWEBRTC_ARCH_ARM
    -DWEBRTC_ARCH_ARM_V7 -DWEBRTC_HAS_NEON -DWEBRTC_POSIX -DWEBRTC_MAC
    -DWEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE -DOPUS_FIXED_POINT
    -DUSE_LIBPCI=1 -DUSE_OPENSSL=1 -D__STDC_CONSTANT_MACROS
    -D__STDC_FORMAT_MACROS -DNDEBUG -DNVALGRIND
    -DDYNAMIC_ANNOTATIONS_ENABLED=0 -DNS_BLOCK_ASSERTIONS=1
    -D_FORTIFY_SOURCE=2
)
set(WEBRTC_DEFS_WIN
    -DCLD_VERSION=2 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DWIN32 -D_WINDOWS
	-DNOMINMAX -DPSAPI_VERSION=1
    -D_CRT_RAND_S -DCERT_CHAIN_PARA_HAS_EXTRA_FIELDS -DWIN32_LEAN_AND_MEAN
    -D_ATL_NO_OPENGL -D_SECURE_ATL -D_HAS_EXCEPTIONS=0
    -D_WINSOCK_DEPRECATED_NO_WARNINGS -DCR_CLANG_REVISION=259396-1 -DUSE_AURA=1
	-DUSE_DEFAULT_RENDER_THEME=1
    -DENABLE_MEDIA_ROUTER=1 -DENABLE_HIDPI=1
    -DENABLE_TOPCHROME_MD=1 -DFIELDTRIAL_TESTING_ENABLED -DNO_TCMALLOC 
    -D__STD_C -D_CRT_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_DEPRECATE 
    -DNTDDI_VERSION=0x0A000000 -D_USING_V110_SDK71_
    -DENABLE_THEMES=1 $
    -DENABLE_BACKGROUND=1 -DENABLE_APP_LIST=1 
    -DENABLE_MDNS=1 -DENABLE_SERVICE_DISCOVERY=1 $
    -DV8_USE_EXTERNAL_STARTUP_DATA -DWEBRTC_WIN -DUSE_LIBPCI=1 -DUSE_OPENSSL=1 
    -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DNDEBUG $
    -DNVALGRIND -DDYNAMIC_ANNOTATIONS_ENABLED=0 -D_UNICODE -DUNICODE
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
#Override the system openssl
set(WEBRTC_SYSINCLUDES_MACOS /usr/local/opt/openssl/include)

set(WEBRTC_SYSINCLUDES_IOS)

set(WEBRTC_CFLAGS
    -Wno-unknown-warning-option
    -fstack-protector --param=ssp-buffer-size=4
    -fno-strict-aliasing -Wno-unused-parameter
    -Wno-missing-field-initializers -pipe -fPIC
)
if (CMAKE_COMPILER_ID STREQUAL "GNU")
    list(APPEND WEBRTC_CLAGS -Wno-unused-local-typedefs)
endif()

set(WEBRTC_CXXFLAGS
    -Wno-unknown-warning-option
    -fno-threadsafe-statics
    -Wno-deprecated -Wno-narrowing
    -Wno-inconsistent-missing-override
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

set(WEBRTC_CXXFLAGS_ANDROID -Wsign-compare -Wno-non-virtual-dtor -Woverloaded-virtual -Wno-abi)

set(WEBRTC_CFLAGS_COMMON_MACOS
    -gdwarf-2 -Wnewline-eof
    -Wendif-labels
    -Wno-selector-type-mismatch -Wheader-hygiene -Wno-char-subscripts
    -Wno-unneeded-internal-declaration -Wno-covered-switch-default
    -Wstring-conversion -Wno-c++11-narrowing -Wno-deprecated-register
)

set(WEBRTC_CFLAGS_MACOS
    ${WEBRTC_CFLAGS_COMMON_MACOS}
    -std=c99 -fno-slp-vectorize -fcolor-diagnostics
)
set(WEBRTC_CXXFLAGS_MACOS
    ${WEBRTC_CFLAGS_COMMON_MACOS}
    -fno-slp-vectorize -fcolor-diagnostics -fno-strict-aliasing
    -fobjc-call-cxx-cdtors #this applies only for obj-cxx, but cmake does not have flags specially for obj-c++
)

set(WEBRTC_CFLAGS_IOS ${WEBRTC_CFLAGS_MACOS})
set(WEBRTC_CXXFLAGS_IOS ${WEBRTC_CXXFLAGS_MACOS})

set(WEBRTC_CFLAGS_WIN
    /wd4091 /wd4127 /wd4351 /wd4355 /wd4503 /wd4589 /wd4611 /wd4100 
    /wd4121 /wd4244 /wd4505 /wd4510 /wd4512 /wd4610 /wd4838 /wd4995 /wd4996 
    /wd4456 /wd4457 /wd4458 /wd4459 /wd4312 /wd4702 /wd4800 /O2 /Ob2 /GF 
    /Oy- /fp:precise /W3 /WX /Zi /GR- /Gy /GS /MT /bigobj /Zc:sizedDealloc- 
    /d2Zi+ /Zc:inline /Oy- /Gw /FS
)
set(WEBRTC_CXXFLAGS_WIN ${WEBRTC_CFLAGS_WIN} /TP)

#Clear cache entry because it will get duplicated otherwise
set(WEBRTC_LDFLAGS "")

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
set(WEBRTC_LDFLAGS_MACOS
    -Wl,-search_paths_first -Wl,-ObjC
    -Wl,-dead_strip
)
set(WEBRTC_LDFLAGS_IOS ${WEBRTC_LDFLAGS_IOS})
set(WEBRTC_LDFLAGS_WIN
    /MACHINE:X86 "/LIBPATH:\"C:/Program Files (x86)/Windows Kits/10/Lib/10.10240.0/ucrt/x86\""
	/DELAYLOAD:dbghelp.dll
    /DELAYLOAD:dwmapi.dll /DELAYLOAD:shell32.dll /DELAYLOAD:uxtheme.dll 
    /maxilksize:0x7ff00000 /safeseh
    /dynamicbase /ignore:4199 /ignore:4221 /nxcompat /largeaddressaware
    /SUBSYSTEM:WINDOWS,5.01 /INCREMENTAL:NO /FIXED:NO /OPT:REF /OPT:ICF 
    /PROFILE
)

set(WEBRTC_SYSLIBS "")
set(WEBRTC_SYSLIBS_ANDROID log GLESv2 OpenSLES m dl)

set(WEBRTC_SYSLIBS_LINUX
    crypto expat pthread Xext m X11 dl
)

set(WEBRTC_SYSLIBS_MACOS
    "-framework AVFoundation" "-framework WebKit" "-framework Cocoa" "-framework AudioToolbox"
    "-framework Foundation" "-framework IOKit" "-framework Security"
    "-framework SystemConfiguration" "-framework CoreAudio" "-framework CoreVideo"
    "-framework CoreMedia" "-framework OpenGL" "-framework QTKit"
)
set(WEBRTC_SYSLIBS_IOS ${WEBRTC_SYSLIBS_MACOS})
set(WEBRTC_SYSLIBS_WIN ssleay32.lib libeay32.lib zlib.lib wininet.lib
    dnsapi.lib version.lib msimg32.lib ws2_32.lib 
    usp10.lib psapi.lib dbghelp.lib winmm.lib shlwapi.lib kernel32.lib 
    gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib 
    oleaut32.lib user32.lib uuid.lib odbc32.lib odbccp32.lib delayimp.lib 
    credui.lib netapi32.lib dmoguids.lib wmcodecdspuuid.lib amstrmid.lib
    msdmo.lib crypt32.lib iphlpapi.lib secur32.lib Strmiids.lib
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
    set(platform Android)
    appendArchStuff(ARM)
    appendPlatformStuff(ANDROID)
elseif(APPLE AND NOT APPLE_IOS)
    set(platform MacOSX)
    # appendArchStuff(X86)
    appendPlatformStuff(MACOS)
elseif (APPLE_IOS)
   set(platform iOS)
    #appendArchStuff(ARM)
    appendPlatformStuff(IOS)
elseif(UNIX) #linux really
    set(platform "x86 Linux")
    appendArchStuff(X86)
    appendPlatformStuff(LINUX)
elseif(WIN32)
    set(platform "x86 Windows")
	appendPlatformStuff(WIN)
else()
    message(FATAL_ERROR "Unknown platform")
endif()
message(STATUS "========================================================")
message(STATUS "Building rtcModule for ${platform}")
message(STATUS "========================================================")

set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${WEBRTC_LDFLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${WEBRTC_LDFLAGS}")

if(optWebrtcShared)
add_library(webrtc_my SHARED dummy.cpp)
else()
add_library(webrtc_my dummy.cpp)
endif()

# Tell ld to not rely on library order to resolve symbols
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    list(INSERT WEBRTC_LIBS 0 -Wl,--start-group)
    list(APPEND WEBRTC_LIBS -Wl,--end-group)
endif()

target_link_libraries(webrtc_my ${WEBRTC_LIBS} ${WEBRTC_SYSLIBS})

set(WEBRTC_CFLAGS "${WEBRTC_CFLAGS}" CACHE INTERNAL "" FORCE)
set(WEBRTC_CXXFLAGS "${WEBRTC_CXXFLAGS}" CACHE INTERNAL "" FORCE)
set(WEBRTC_SYSLIBS ${WEBRTC_SYSLIBS} CACHE INTERNAL "" FORCE)
set(WEBRTC_LDFLAGS "-fno-rtti" CACHE INTERNAL "" FORCE)
set(WEBRTC_DEFS ${WEBRTC_DEFS} CACHE INTERNAL "" FORCE)
set(WEBRTC_INCLUDES ${WEBRTC_INCLUDES} CACHE INTERNAL "" FORCE)

