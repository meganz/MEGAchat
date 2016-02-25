#!/bin/bash
set -e
# The default revision of the webrtc repository to use, unlesss overridded with --revision
# This revision is giaranteed to work with the current state of the karere-native and webrtc-build codebase
revision='e2d83d6560272ee68cf99c4fd4f78a437adeb98c'

if (( $# < 1 )); then
   echo "Not enough arguments"
   echo "Usage: $(basename $0) <webrtc-output-dir> [--deproot <prefix-of-dependencies>>] [--revision <rev>] [--batch]"
   exit 1
fi

echo "========================================================="
platform=`uname`
echo "Platform: $platform"
karere=`echo "$(cd "$(dirname "$0")"; pwd)"`
echo "Karere webrtc-build directory: $karere"
webrtcdir=$1
echo "Webrtc directory: $webrtcdir"
shift
buildtype="Release"

while [[ $# > 0 ]]
do
    key="$1"
    case $key in
    -s|--deproot)
        if [[ $# < 2 ]]; then
           echo "No dependency prefix directory specified adter --deproot"
           exit 1
        fi
        deproot=$2
        shift
        echo "Prefix where dependencies will be searched in: $deproot"
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

echo "WebRTC revision: '$revision'"
echo "WebRTC build type: $buildtype"

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
else
    echo "depot_tools seems already downloaded"
fi

export PATH="$webrtcdir/depot_tools:$PATH"

if [ -f ./.gclient ] && [ -f src/DEPS ]; then
    echo "Webrtc source tree seems already checked out"
    cd src
else
    echo "Configuring gclient for webrtc repo..."
    gclient config --name src --unmanaged https://dummy
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
$karere/get-chromium-deps.sh "$chromiumRev"

echo "Replacing boringssl..."
mkdir -p ./chromium/src/third_party/boringssl
cp -v $karere/boringssl.gyp ./chromium/src/third_party/boringssl/

python $karere/link-chromium-deps.py
echo "==========================================================="

echo "Setting platform-independent env variables..."
if [ ! -z "$deproot" ]; then
    export WEBRTC_DEPS_LIB=$deproot/lib
    export WEBRTC_DEPS_INCLUDE=$deproot/include
fi
export GYP_GENERATORS=ninja

echo "Patching base.gyp..."
pwd
git checkout --force ./webrtc/base/base.gyp
git apply "$karere/base.gyp.patch"


if [[ $platform == "Darwin" ]]; then
    echo "Performing MacOS-specific operations"

    echo "Setting GYP_DEFINES..."
    export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 OS=mac target_arch=x64 clang_use_chrome_plugins=0 mac_deployment_target=10.7 use_openssl=1 use_nss=0 include_tests=0"

    echo "Replacing macos capturer..."
    rm -rf webrtc/modules/video_capture/mac
    cp -rv "$karere/macos/mac-capturer" "webrtc/modules/video_capture/mac"
    if [ ! -f ./.patched-mac-capturer ]; then
        echo "Applying patch for capturer"
        git apply "$karere/macos/capturer.patch"
        touch ./.patched-mac-capturer
    fi
    if [ ! -f ./.patched-common.gypi ]; then
        echo "Applyting patch to common.gypi"
        git apply "$karere/macos/common.gypi.patch"
        touch ./.patched-common.gypi
    fi
elif [[ "$platform" == "Linux" ]]; then
    echo "Setting GYP_DEFINES..."
    export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 enable_tracing=1 clang=0 use_openssl=1 use_nss=0 use_sysroot=0 include_tests=0"

else
    echo "Non-mac platforms are not supported by this script yet"
    exit 1
fi

echo "Generating ninja makefiles..."
gclient runhooks --force

echo "Building webrtc in release mode..."
ninja -C out/$buildtype

echo "All done"


