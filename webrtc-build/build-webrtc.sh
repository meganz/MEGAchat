#!/bin/bash
set -e
# The default revision of the webrtc repository to use, unlesss overridded with --revision
# This revision is giaranteed to work with the current state of the karere-native and webrtc-build codebase
#revision='e2d83d6560272ee68cf99c4fd4f78a437adeb98c' - on ios this requires XCode 7, so we use a revision that is a bit (1 day) older
revision='9ac4df1ba66d39c3621cfb2e8ed08ae39658b793'

if (( $# < 2 )); then
   echo "Not enough arguments"
   echo "Usage: $(basename $0) <webrtc-output-dir> --platform <linux|macos|ios|android> [--deproot <prefix-of-dependencies>>] [--revision <rev>] [--batch]"
   exit 1
fi
function checkPlatformValid
{
  if [ -z "$platform" ]; then
      echo "No platform specified (--platform=xxx)"
      return 1
  elif [[ "$platform" != linux ]] && [[ "$platform" != macos ]] && [[ "$platform" != "android" ]] && [[ "$platform" != "ios" ]] && [[ "$platform" != "win" ]]; then
      echo -e "Invalid platform \033[1;31m'$platform'\033[0;0m"
      echo "Valid platforms are: linux, macos, ios, android, win"
      return 1
  fi
}

karere=`echo "$(cd "$(dirname "$0")"; pwd)"`
echo "======================================================================="
webrtcdir=$1
shift
buildtype="Release"

while [[ $# > 0 ]]
do
    key="$1"
    case $key in
    --deproot)
        if [[ $# < 2 ]]; then
           echo "No dependency prefix directory specified adter --deproot"
           exit 1
        fi
        deproot=$2
        shift
        echo "Prefix for dependencies: $deproot"
        ;;
    -p|--platform)
       if [[ $# < 2 ]]; then
          echo "No platform specified"
          exit 1
       fi
       platform=$2
       shift
       ;;
    -b|--batch)
        batch=1
        ;;
    -r|--revision)
        if [[ $# < 2 ]]; then
           echo "No revision specified"
           exit 1
        fi
        revision="$2"
        shift # skip revision value
        ;;
    -d|--debug)
        buildtype="Debug"
        ;;
    *)
        echo "Unknown option '$1'"
        exit 1
        ;;
    esac
    shift # past argument or value
done

checkPlatformValid
echo -e "Platform: \033[0;32m${platform}\033[0;0m"
echo "Script directory: $karere"
echo "WebRTC revision: '$revision'"
echo "WebRTC build type: $buildtype"
echo "WebRTC directory: $webrtcdir"

if [[ "$platform" == "win" ]]; then
    if [[ `uname -o` != "Cygwin" ]]; then
	    echo "On Windows, you must run this script under Cygwin"
		exit 1
	fi
#setup cygwin
#by default, symlinks created by cygwin are only undestoob by cygwin, and we need native tools to also
#understand them, so set symlink mode to native.NOTE: This requires that you run the cygwin shell with
#Admin privileges
    (id -G | grep -qE '\<(544|0)\>') || (echo "You need to run the Cygwin shell as Administrator to be able to create native symlinks" && exit 1)

    if [[ -z "$CYGWIN" ]]; then
        export CYGWIN="winsymlinks:nativerestrict"
    else
	    export CYGWIN="$CYGWIN winsymlinks:nativerestrict"
    fi
fi

if [[ -z $batch ]]; then
    read -n1 -r -p "Press enter to continue if these paths are ok, or ctrl+c to abort..." key
fi

echo "========================================================="

export KR_WEBRTC_BUILD=$karere

if [ -d $webrtcdir ]; then
    echo "Webrtc directory already exists"
else
    mkdir -p $webrtcdir
fi

cd $webrtcdir
webrtcdir=`pwd`
if [ ! -d ./depot_tools ]; then
    echo "Checking out depot_tools..."
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    if [[ "$platform" == "win" ]]; then
       echo "Downloading native python..."
       $karere/win/getpython.sh `pwd`/depot_tools
       #some native stuff needs to call native python, but we don't want native
       #python in the unix path - the bat file is ignored by bash, but picked
       #by native shell
       cp -v $karere/win/python.bat `pwd`/depot_tools
       echo "Patching gclient to use native python..."
       cd depot_tools
       git apply $karere/win/gclient.patch
       cd ..
    fi
else
    echo "depot_tools seems already downloaded"
fi


export PATH="$webrtcdir/depot_tools:$PATH"
if [[ "$platform" == "android" ]]; then
    export gclientConfigOpts="target_os=['android', 'unix']"
fi

if [ -f ./.gclient ] && [ -f src/DEPS ]; then
    echo "Webrtc source tree seems already checked out"
    cd src
else
    echo "Configuring gclient for webrtc repo..."
    gclient config --name src --unmanaged https://dummy
    if [ ! -z "$gclientConfigOpts" ]; then
        echo "$gclientConfigOpts" >> ./.gclient
    fi
 
    if [ ! -d src ]; then
        mkdir -p src
    fi
    cd src
    if [ ! -d ./.git ]; then
        git init
        git remote add origin https://chromium.googlesource.com/external/webrtc.git
    fi
    echo "Fetching webrtc git repository at revision '$revision'..."
    git fetch --depth=1 origin $revision
    git checkout $revision

    echo "Removing unneccessary deps from webrtc DEPS file..."
    python "$karere/remove-from-DEPS.py" ./DEPS "$karere/webrtc-deps-del.txt"
fi

contents=$(<./DEPS)
regex="'chromium_revision': '([^']+)',"
[[ "$contents" =~ $regex ]]
chromiumRev="${BASH_REMATCH[1]}"
if [ -z "$chromiumRev" ]; then
    echo "Error extracting required chromium revision from DEPS file"
    exit 1
fi
echo "Required chromium revision for this webrtc is $chromiumRev"

echo "==========================================================="
echo "Selecticely downloading build scripts from chromium repo..."
$karere/get-chromium-deps.sh "$platform" "$chromiumRev"

echo "Replacing boringssl..."
mkdir -p ./chromium/src/third_party/boringssl
if [[ "$platform" == "win" ]]; then
  boringSslFile=win/boringssl-win.gyp
else
  boringSslFile=boringssl.gyp
fi
cp -v $karere/$boringSslFile ./chromium/src/third_party/boringssl/boringssl.gyp

echo "==========================================================="

echo "Setting platform-independent env variables..."
if [ ! -z "$deproot" ]; then
    if [[ "$platform" == "win" ]]; then
      export WEBRTC_DEPS_LIB=`cygpath -w $deproot/lib`
      export WEBRTC_DEPS_INCLUDE=`cygpath -w $deproot/include`
    else
      export WEBRTC_DEPS_LIB=$deproot/lib
      export WEBRTC_DEPS_INCLUDE=$deproot/include
    fi
fi
export GYP_GENERATORS=ninja

echo "Patching base.gyp..."
pwd
git checkout --force ./webrtc/base/base.gyp
git apply "$karere/base.gyp.patch"

export GYP_DEFINES="include_tests=0 build_with_libjingle=1 build_with_chromium=0 clang_use_chrome_plugins=0 use_openssl=1 use_nss=0 enable_tracing=1"
if [[ $platform == "macos" ]]; then
    echo "Performing MacOS-specific operations"

    echo "Setting GYP_DEFINES..."
    export GYP_DEFINES="$GYP_DEFINES OS=mac libjingle_objc=1 target_arch=x64 mac_deployment_target=10.7"

    echo "Replacing macos capturer..."
    rm -rf webrtc/modules/video_capture/mac
    cp -rv "$karere/macos/mac-capturer" "webrtc/modules/video_capture/mac"
    if [ ! -f ./.patched-mac-capturer ]; then
        echo "Applying patch for capturer"
        git apply "$karere/macos/capturer.patch"
        touch ./.patched-mac-capturer
    fi
    if [ ! -f ./.patched-common.gypi ]; then
        echo "Applying patch to common.gypi"
        git apply "$karere/macos/common.gypi.patch"
        touch ./.patched-common.gypi
    fi
elif [[ "$platform" == "linux" ]]; then
    echo "Setting GYP_DEFINES for Linux..."
    export GYP_DEFINES="$GYP_DEFINES OS=linux clang=0 use_sysroot=0"
elif [[ "$platform" == "android" ]]; then
    if [ -z "$ANDROID_NDK" ]; then
        echo "ERROR: ANDROID_NDK is not set. Please set it to the NDK root dir and re-run this script"
        exit 1
    fi
    echo "Setting GYP_DEFINES for Android..."
    export GYP_DEFINES="$GYP_DEFINES OS=android target_arch=arm arm_version=7 include_examples=0 werror='' never_lint=1"
    echo "Patching libsrtp..."
    (cd chromium/src/third_party/libsrtp; git apply "$karere/android/libsrtp.patch")
    echo "Downloading required Android SDK components..."
    "$karere/android/setup-ndk-sdk.sh"
    echo "Linking Android NDK to chromium tree..."
    rm -f chromium/src/third_party/android_tools/ndk
    ln -sv "$ANDROID_NDK" "chromium/src/third_party/android_tools/ndk"
elif [[ "$platform" == "ios" ]]; then
    echo "Setting GYP_DEFINES for iOS..."
    export GYP_DEFINES="$GYP_DEFINES OS=ios target_arch=arm arm_version=7 libjingle_objc=1 use_system_libcxx=1 ios_deployment_target=7.0"
    export GYP_CROSSCOMPILE=1
elif [[ "$platform" == "win" ]]; then
    export GYP_MSVS_VERSION=2015
    export DEPOT_TOOLS_WIN_TOOLCHAIN=0
    export GYP_LINK_CONCURRENCY=1
else
    echo "Platform '$platform' not supported by this script yet"
    exit 1
fi

python $karere/link-chromium-deps.py

echo "Generating ninja makefiles..."
gclient runhooks --force

echo "Building webrtc in $buildtype mode..."
if [[ "$platform" != "ios" ]]; then
    ninja -C out/$buildtype
else
    ninja -C out/$buildtype-iphoneos AppRTCDemo
fi

echo "All done"


