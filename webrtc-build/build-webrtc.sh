#!/bin/bash
set -e

if (( $# < 2 )); then
   echo "Not enough arguments"
   echo "Usage: $(basename $0) <webrtc-output-dir> <prefix-of-dependencies>"
   exit 1
fi

echo "========================================================="
platform=`uname`
echo "Platform: $platform"
karere=$(dirname "$0")
echo "Karere webrtc-build directory: $karere"
webrtcdir=$1
echo "Webrtc directory: $webrtcdir"
deproot=$2
echo "Prefix where dependencies are installed: $deproot"
if [[ $3 != "-batch" ]]; then
    read -n1 -r -p "Press enter to continue if these paths are ok, or ctrl+c to abort..." key
fi

echo "========================================================="
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

if [ -d src ]; then
    echo "Webrtc main source tree seems already checked out"
else
    echo "Fetching main webrtc source tree..."
    fetch --no-history --nohooks webrtc
fi

echo "Syncing webrtc subtrees. This may take a lot of time..."
gclient sync --force --revision 290ab41

cd src

echo "Replacing boringssl.gyp..."
rm -rf ./third_party/boringssl
mkdir ./third_party/boringssl
cp "$karere/boringssl.gyp" ./third_party/boringssl/

echo "Setting platform-independent env variables..."
export DEPS_SYSROOT=$deproot
export GYP_GENERATORS=ninja

echo "Patching base.gyp..."
git checkout --force webrtc/base/base.gyp
git apply "$karere/base.gyp.patch"

if [[ $platform == "Darwin" ]]; then
    echo "Performing MacOS-specific operations"

    echo "Setting GYP_DEFINES..."
    export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 OS=mac target_arch=x64 clang_use_chrome_plugins=0 mac_deployment_target=10.7 use_openssl=1 use_nss=0"
    echo "Replacing macos capturer..."
    rm -rf webrtc/modules/video_capture/mac
    cp -rv "$karere/macos/mac-capturer" "webrtc/modules/video_capture/mac"
    echo "Applying patch for capturer and other mac-specific things"
    git apply "$karere/macos/webrtc.patch"
else
    echo "Non-mac platforms are not supported by this script yet"
    exit 1
fi

echo "Generating ninja makefiles..."
gclient runhooks --force

echo "Building webrtc in release mode..."
ninja -C out/Release

echo "All done"


